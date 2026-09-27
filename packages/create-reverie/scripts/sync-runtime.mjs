// Copy the repo's authoritative RingEcho runtime (root src/) into the package's
// bundled runtime/, so a published create-reverie is self-contained.

import { cpSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const PKG_ROOT = resolve(HERE, "..");
const REPO_ROOT = resolve(PKG_ROOT, "..", "..");
const SOURCE = join(REPO_ROOT, "src");
const DEST = join(PKG_ROOT, "runtime");

rmSync(DEST, { recursive: true, force: true });
cpSync(SOURCE, DEST, { recursive: true });
console.log(`synced runtime: ${SOURCE} -> ${DEST}`);
