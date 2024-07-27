#[derive(rquickjs::class::Trace)]
#[rquickjs::class]
pub struct Console {
  foo: u32,
}

#[rquickjs::methods]
impl Console {
  #[qjs(constructor)]
  pub fn new() -> Self {
    Self { foo: 3 }
  }

  pub fn log(&self, message: String) {
    println!("{}", message);
  }
}

impl Default for Console {
  fn default() -> Self {
    Self::new()
  }
}

/// Ref: https://docs.rs/rquickjs/latest/rquickjs/attr.module.html
#[rquickjs::module(rename_vars = "camelCase")]
pub mod console_module {
  use rquickjs::Ctx;

  pub use super::Console;

  #[qjs(declare)]
  pub fn declare(declare: &rquickjs::module::Declarations) -> rquickjs::Result<()> {
    declare.declare("console")?;
    Ok(())
  }

  #[qjs(evaluate)]
  pub fn evaluate<'js>(
    _ctx: &Ctx<'js>,
    exports: &rquickjs::module::Exports<'js>,
  ) -> rquickjs::Result<()> {
    exports.export("console", Console::new())?;
    Ok(())
  }
}
