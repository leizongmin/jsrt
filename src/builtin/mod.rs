mod console;

use anyhow::Result;
use rquickjs::{Context, Module};

use console::js_console_module;

pub fn init_modules(ctx: Context) -> Result<()> {
  ctx.with(|ctx| {
    Module::declare_def::<js_console_module, _>(ctx.clone(), "jsrt:console")?;
    Ok(())
  })
}
