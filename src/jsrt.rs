use anyhow::Result;
use rquickjs::{qjs, Context, Ctx, Runtime, Value};
use std::ffi::{CStr, CString};
use std::mem;
use std::ptr::NonNull;

/// The main JSRT runtime object
pub struct Core {
  #[allow(dead_code)]
  rt: Runtime,
  ctx: Context,
}

/// Error type for JSRT
#[derive(thiserror::Error, Debug)]
pub enum Error {
  /// An exception was thrown in the JS code
  #[error("{}: {}\n{}", .0.name, .0.message, .0.stack)]
  Exception(Exception),

  /// An unknown error occurred
  #[allow(unused_variables)]
  #[error("unknown error")]
  Unknown(anyhow::Error),
}

/// An exception thrown in JS code
#[derive(Debug)]
pub struct Exception {
  /// The name of the exception
  pub name: String,
  /// The message of the exception
  pub message: String,
  /// The stack trace of the exception
  pub stack: String,
}

impl Core {
  /// Create a new JSRT runtime
  pub fn new() -> Result<Self> {
    let rt = Runtime::new()?;
    let ctx = Context::full(&rt)?;

    rt.set_memory_limit(1024 * 1024 * 1024);
    rt.set_max_stack_size(1024 * 1024);

    // let resolver = (FileResolver::default().with_path("./").with_native(),);
    // let loader = (ScriptLoader::default(), NativeLoader::default());
    // rt.set_loader(resolver, loader);
    crate::builtin::init_modules(ctx.clone())?;

    Ok(Self { rt, ctx })
  }

  /// Evaluate a JS script without a filename
  /// The callback will be called with the result of the script
  pub fn eval_anonymous<S, F>(&self, source: S, callback: F) -> Result<()>
  where
    S: Into<Vec<u8>>,
    F: FnOnce(Ctx, Value) -> Result<()>,
  {
    self.eval("anonymous", source, callback)
  }

  /// Evaluate a JS script with a filename
  /// The callback will be called with the result of the script
  /// If an exception is thrown in the script, an Error::Exception will be returned
  /// with the exception details
  pub fn eval<S, F>(&self, filename: &str, source: S, callback: F) -> Result<()>
  where
    S: Into<Vec<u8>>,
    F: FnOnce(Ctx, Value) -> Result<()>,
  {
    self.ctx.with(|ctx| {
      let src = source.into();
      let src_len = src.len();
      let src = CString::new(src)?;
      let is_module = filename.ends_with(".mjs")
        || unsafe { qjs::JS_DetectModule(src.as_ptr(), src_len as _) == 1 };

      let mut eval_flags = if is_module {
        qjs::JS_EVAL_TYPE_MODULE
      } else {
        qjs::JS_EVAL_TYPE_GLOBAL
      };
      eval_flags |= qjs::JS_EVAL_FLAG_STRICT;
      eval_flags |= qjs::JS_EVAL_FLAG_ASYNC;

      let filename = str_to_c_char(filename)?;

      let val = unsafe {
        qjs::JS_Eval(
          self.ctx.as_raw().as_ptr(),
          src.as_ptr(),
          src_len as _,
          filename,
          eval_flags as _,
        )
      };

      let is_exception = unsafe { qjs::JS_IsException(val) };
      if is_exception {
        let err = Error::Exception(Exception::from_raw_context(self.ctx.as_raw())?);
        Err(err.into())
      } else {
        callback(ctx.clone(), unsafe { Value::from_raw(ctx.clone(), val) })?;
        unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), val) };
        Ok(())
      }
    })
  }

  pub fn execute_pending_job(&self, ctx: Ctx) -> Result<bool> {
    let mut ctx_ptr = mem::MaybeUninit::<*mut qjs::JSContext>::uninit();
    let result = unsafe {
      qjs::JS_ExecutePendingJob(
        qjs::JS_GetRuntime(ctx.as_raw().as_ptr()),
        ctx_ptr.as_mut_ptr(),
      )
    };
    if result == 0 {
      // no jobs executed
      return Ok(false);
    }
    if result == 1 {
      // single job executed
      return Ok(true);
    }
    let exc = unsafe { qjs::JS_GetException(ctx.as_raw().as_ptr()) };
    let err = Error::Exception(Exception::from_raw_context(ctx.as_raw())?);
    unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), exc) };
    Err(err.into())
  }

  pub fn await_promise<F>(&self, ctx: Ctx, p: Value, callback: F) -> Result<()>
  where
    F: FnOnce(Ctx, PromiseState) -> Result<bool> + Copy,
  {
    loop {
      let state = unsafe { qjs::JS_PromiseState(self.ctx.as_raw().as_ptr(), p.as_raw()) };
      match state {
        qjs::JSPromiseStateEnum_JS_PROMISE_FULFILLED => {
          eprintln!("promise fulfilled");
          let result_raw = unsafe { qjs::JS_PromiseResult(self.ctx.as_raw().as_ptr(), p.as_raw()) };
          let result = unsafe { Value::from_raw(ctx.clone(), result_raw) };
          unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), result_raw) };
          callback(ctx.clone(), PromiseState::Fulfilled(result))?;
          return Ok(());
        }
        qjs::JSPromiseStateEnum_JS_PROMISE_REJECTED => {
          eprintln!("promise rejected");
          let reason_val = unsafe { qjs::JS_PromiseResult(self.ctx.as_raw().as_ptr(), p.as_raw()) };
          let throw_val = unsafe { qjs::JS_Throw(ctx.as_raw().as_ptr(), reason_val) };
          let err = Error::Exception(Exception::from_raw_context(self.ctx.as_raw())?);
          unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), throw_val) };
          unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), reason_val) };
          callback(ctx.clone(), PromiseState::Rejected(err))?;
          return Ok(());
        }
        qjs::JSPromiseStateEnum_JS_PROMISE_PENDING => {
          eprintln!("promise pending");
          if !callback(ctx.clone(), PromiseState::Pending)? {
            return Ok(());
          }
        }
        _ => {
          callback(ctx.clone(), PromiseState::Fulfilled(p))?;
          return Ok(());
        }
      }
    }
  }
}

#[derive(Debug)]
pub enum PromiseState<'a> {
  Fulfilled(Value<'a>),
  Rejected(Error),
  Pending,
}

impl Exception {
  /// Create a new exception from a raw JSContext
  /// This will consume the JSContext and free the exception value
  fn from_raw_context(ctx: NonNull<qjs::JSContext>) -> Result<Self> {
    let exc = unsafe { qjs::JS_GetException(ctx.as_ptr()) };
    let exc_str = unsafe { qjs::JS_ToCString(ctx.as_ptr(), exc) };

    let mut name = String::new();
    let mut message = String::new();
    let mut stack = String::new();

    if let Some(s) = unsafe { get_js_property_str(ctx.clone(), exc, "name") }? {
      name = s.to_string();
    }
    if let Some(s) = unsafe { get_js_property_str(ctx.clone(), exc, "message") }? {
      message = s.to_string();
    }
    if let Some(s) = unsafe { get_js_property_str(ctx.clone(), exc, "stack") }? {
      stack = s.to_string();
    }

    unsafe { qjs::JS_FreeCString(ctx.as_ptr(), exc_str as *const i8) };
    unsafe { qjs::JS_FreeValue(ctx.as_ptr(), exc) };

    Ok(Self {
      name,
      message,
      stack,
    })
  }
}

/// Convert a C string to a Rust string
unsafe fn cstr_to_string(ptr: *const i8) -> std::result::Result<String, std::str::Utf8Error> {
  let c_str = CStr::from_ptr(ptr);
  let str_slice = c_str.to_str()?;
  Ok(str_slice.to_string())
}

/// Convert a Rust string to a C string
fn str_to_c_char(s: &str) -> Result<*const i8> {
  let c_string = CString::new(s)?;
  Ok(c_string.as_ptr())
}

/// Get a JS property as a string
unsafe fn get_js_property_str(
  ctx: NonNull<qjs::JSContext>,
  val: qjs::JSValue,
  prop: &str,
) -> Result<Option<String>> {
  let mut result = None;
  let prop_name = CString::new(prop)?;
  let prop_val = unsafe { qjs::JS_GetPropertyStr(ctx.as_ptr(), val, prop_name.as_ptr()) };
  if unsafe { qjs::JS_IsString(prop_val) } {
    let prop_str = unsafe { qjs::JS_ToCString(ctx.as_ptr(), prop_val) };
    result = unsafe { Some(cstr_to_string(prop_str)?) };
    unsafe { qjs::JS_FreeCString(ctx.as_ptr(), prop_str as *const i8) };
  }
  unsafe { qjs::JS_FreeValue(ctx.as_ptr(), prop_val) };
  Ok(result)
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn test_jsrt() {
    let rt = Core::new().unwrap();

    for _ in 0..3 {
      rt.eval_anonymous("await 1", |ctx, val| {
        println!("result: {:?}", val);
        let val = rt.await_promise(ctx, val, |ctx, state| {
          println!("state: {:?}", state);
          // let has_job = rt.execute_pending_job(ctx)?;
          // println!("has_job: {:?}", has_job);
          // Ok(has_job)
          Ok(false)
        })?;
        println!("result: {:?}", val);
        Ok(())
      })
      .unwrap();
    }

    for _ in 0..2 {
      match rt.eval_anonymous("throw new Error('hello');", |ctx, val| {
        println!("result: {:?}", val);
        let val = rt.await_promise(ctx, val, |ctx, val| {
          println!("state: {:?}", val);
          Ok(false)
        })?;
        println!("result: {:?}", val);
        Ok(())
      }) {
        Ok(_) => {}
        Err(e) => {
          println!("error: {:?}", e);
        }
      }
    }

    // rt.eval_anonymous(
    //   r"
    //   import { console } from 'jsrt:console';
    //   console.log('hello');
    // ",
    //   |ctx, val| {
    //     println!("result: {:?}", val);
    //     let val = rt.await_promise(ctx, val)?;
    //     println!("result: {:?}", val);
    //     Ok(())
    //   },
    // )
    // .unwrap();
  }
}
