use anyhow::{anyhow, Result};
use clap::{Parser, Subcommand};
use std::path::PathBuf;

mod builtin;
mod jsrt;

/// Welcome to jsrt, a small JavaScript runtime.
#[derive(Parser)]
#[command(version, about, long_about = None)]
struct Cli {
  /// Script file to run
  script_file: Option<PathBuf>,
  #[command(subcommand)]
  command: Option<Commands>,
  /// Arguments for script
  args: Vec<String>,
}

#[derive(Subcommand)]
enum Commands {
  /// Create self-contained binary file
  Build {
    script_file: PathBuf,
    target_file: Option<PathBuf>,
  },
  /// Start REPL
  REPL { script_file: Option<PathBuf> },
  /// Run script from argument and print the result
  Eval { script_code: String },
  /// Run script from argument
  Exec { script_code: String },
}

fn main() {
  let cli = Cli::parse();
  let ret = match cli.command {
    Some(Commands::Build {
      script_file,
      target_file,
    }) => Err(anyhow!(
      "unimplemented! Build: {:?} {:?}",
      script_file,
      target_file
    )),
    Some(Commands::REPL { script_file }) => Err(anyhow!(
      "unimplemented! REPL: {:?} {:?}",
      script_file,
      cli.args
    )),
    Some(Commands::Eval { script_code }) => Err(anyhow!(
      "unimplemented! Eval: {:?} {:?}",
      script_code,
      cli.args
    )),
    Some(Commands::Exec { script_code }) => Err(anyhow!(
      "unimplemented! Exec: {:?} {:?}",
      script_code,
      cli.args
    )),
    None => match cli.script_file {
      Some(script_file) => run_script_file(script_file.to_string_lossy().to_string()),
      None => Err(anyhow!("unimplemented! REPL")),
    },
  };
  if let Err(e) = ret {
    eprintln!("Error: {}", e);
    std::process::exit(1);
  }
}

fn run_script_file(script_file: String) -> Result<()> {
  let source = std::fs::read_to_string(&script_file)?;
  let core = jsrt::Core::new()?;
  core.eval(&script_file, source.as_str(), |_ctx, val| {
    println!("result: {:?}", val);
    Ok(())
  })?;
  Ok(())
}
