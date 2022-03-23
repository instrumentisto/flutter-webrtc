#![warn(clippy::pedantic)]

use std::{env, fs, path::Path};

use anyhow::{anyhow, Context};
use walkdir::{DirEntry, WalkDir};

fn main() -> anyhow::Result<()> {
    cxx_build::bridge("src/cpp_api.rs").compile("cpp_api_bindings");

    copy_cxxbridge1_lib()?;

    Ok(())
}

/// Finds and copies the compiled `cxxbridge1` static library to the
/// `target/cxxbridge/` directory.
fn copy_cxxbridge1_lib() -> anyhow::Result<()> {
    let out_dir = env::var_os("OUT_DIR")
        .ok_or_else(|| anyhow!("`OUT_DIR` environment variable not found"))?;
    let out_dir_path = Path::new(&out_dir);
    let cxxbridge_dir = out_dir_path
        .ancestors()
        .nth(4)
        .ok_or_else(|| anyhow!("Could not find `target/` directory"))?
        .join("cxxbridge");
    let build_dir = out_dir_path.ancestors().nth(2).ok_or_else(|| {
        anyhow!("Could not find `target/debug/`|`release/build/` directory",)
    })?;
    let target_env = env::var("CARGO_CFG_TARGET_ENV")?;
    let lib_name = if target_env == "msvc" {
        "cxxbridge1.lib"
    } else {
        "libcxxbridge1.a"
    };

    let mut libs: Vec<DirEntry> = WalkDir::new(build_dir)
        .into_iter()
        .map(Result::unwrap)
        .filter(|e| e.path().ends_with(lib_name))
        .collect();

    let cxxbridge = libs
        .pop()
        .with_context(|| "Could not find `cxxbridge1` static library.")?;
    if !libs.is_empty() {
        return Err(anyhow!(
            "Found multiple `cxxbridge1` libraries, only one is expected, try \
             running `cargo clean`."
        ));
    }

    fs::copy(cxxbridge.path(), cxxbridge_dir.join(lib_name))?;

    Ok(())
}
