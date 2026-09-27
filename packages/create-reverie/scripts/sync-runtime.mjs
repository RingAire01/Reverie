// Copy the repository's authoritative runtime, shim sources and build scripts
// into the package, so a published create-reverie is self-contained.

import { copyFileSync, cpSync, existsSync, mkdirSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const PKG_ROOT = resolve(HERE, "..");
const REPO_ROOT = resolve(PKG_ROOT, "..", "..");

function syncDir(source, dest) {
    rmSync(dest, { recursive: true, force: true });
    cpSync(source, dest, { recursive: true });
    console.log(`synced ${source} -> ${dest}`);
}

// Reo runtime.
syncDir(join(REPO_ROOT, "src"), join(PKG_ROOT, "runtime"));

// C shim source + its build script (not the fetched SDK).
const shimDest = join(PKG_ROOT, "shim");
rmSync(shimDest, { recursive: true, force: true });
mkdirSync(shimDest, { recursive: true });
for (const name of ["reverie_webview2.c", "reverie_webview2.h", "build.ps1"]) {
    copyFileSync(join(REPO_ROOT, "shim", name), join(shimDest, name));
}
console.log(`synced shim -> ${shimDest}`);

// Build scripts.
const scriptsDest = join(PKG_ROOT, "scripts");
mkdirSync(scriptsDest, { recursive: true });
for (const name of ["fetch-webview2.ps1", "build-app.ps1"]) {
    const source = join(REPO_ROOT, "scripts", name);
    if (existsSync(source)) copyFileSync(source, join(scriptsDest, name));
}
console.log(`synced scripts -> ${scriptsDest}`);
