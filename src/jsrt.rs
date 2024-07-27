use anyhow::Result;
use rquickjs::{qjs, Context, Runtime, Value};
use std::ffi::{CStr, CString};
use std::ptr::NonNull;

/// The main JSRT runtime object
pub struct Core {
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
    Ok(Self { rt, ctx })
  }

  /// Evaluate a JS script without a filename
  /// The callback will be called with the result of the script
  pub fn eval_anonymous<S, F>(&self, source: S, callback: F) -> Result<()>
  where
    S: Into<Vec<u8>>,
    F: FnOnce(Value),
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
    F: FnOnce(Value),
  {
    self.ctx.with(|ctx| {
      let src = source.into();
      let src_len = src.len();
      let src = CString::new(src)?;
      let is_module = filename.ends_with(".mjs")
        || unsafe { qjs::JS_DetectModule(src.as_ptr(), src_len as _) == 1 };

      let eval_flags = if is_module {
        qjs::JS_EVAL_TYPE_MODULE
      } else {
        qjs::JS_EVAL_TYPE_GLOBAL
      };

      let val = unsafe {
        qjs::JS_Eval(
          self.ctx.as_raw().as_ptr(),
          src.as_ptr(),
          src_len as _,
          filename.as_ptr() as *const i8,
          eval_flags as _,
        )
      };

      let is_exception = unsafe { qjs::JS_IsException(val) };
      if is_exception {
        let err = Error::Exception(Exception::from_raw_context(self.ctx.as_raw())?);
        Err(err.into())
      } else {
        callback(unsafe { Value::from_raw(ctx.clone(), val) });
        unsafe { qjs::JS_FreeValue(ctx.as_raw().as_ptr(), val) };
        Ok(())
      }
    })
  }
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

    unsafe {
      qjs::JS_FreeCString(ctx.as_ptr(), exc_str as *const i8);
      qjs::JS_FreeValue(ctx.as_ptr(), exc);
    }

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
    unsafe {
      result = Some(cstr_to_string(prop_str)?);
      qjs::JS_FreeCString(ctx.as_ptr(), prop_str as *const i8);
    }
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

    rt.eval_anonymous("let a = 1+1+3; a", |val| {
      println!("result: {:?}", val);
    })
    .unwrap();

    match rt.eval_anonymous("throw new Error('hello');", |val| {
      println!("result: {:?}", val);
    }) {
      Ok(_) => {}
      Err(e) => {
        println!("error: {:?}", e);
      }
    }
  }
}
