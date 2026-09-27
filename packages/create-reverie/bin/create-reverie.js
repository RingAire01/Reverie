#!/usr/bin/env node
// create-reverie - scaffold a Reverie desktop app.
//
// Layout produced:
//   <app>/src/            web frontend (template specific)
//   <app>/src-reverie/    RingEcho runtime (copied from the bundled runtime/)

import { cpSync, existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const PKG_ROOT = resolve(HERE, "..");
const TEMPLATES = join(PKG_ROOT, "templates");
const RUNTIME = join(PKG_ROOT, "runtime");
const SHIM = join(PKG_ROOT, "shim");
const SCRIPTS = join(PKG_ROOT, "scripts");

export const TEMPLATE_NAMES = readdirSync(TEMPLATES).filter(
  (name) => !name.startsWith("_") && statSync(join(TEMPLATES, name)).isDirectory(),
);

function parseArgs(argv) {
  const options = { template: "react", install: false, force: false, pm: "pnpm" };
  const positional = [];
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--template" || arg === "-t") options.template = argv[++i];
    else if (arg === "--package-manager" || arg === "--pm") options.pm = argv[++i];
    else if (arg === "--install") options.install = true;
    else if (arg === "--force") options.force = true;
    else if (arg === "--help" || arg === "-h") options.help = true;
    else if (arg.startsWith("-")) throw new Error(`unknown option: ${arg}`);
    else positional.push(arg);
  }
  options.target = positional[0];
  return options;
}

function usage() {
  return [
    "create-reverie - scaffold a Reverie desktop app",
    "",
    "Usage:",
    "  create-reverie <dir> [--template <name>] [--pm npm|pnpm|bun] [--install]",
    "",
    `Templates: ${TEMPLATE_NAMES.join(", ")}`,
    "",
    "The command writes a web frontend to <dir>/src and the RingEcho",
    "runtime to <dir>/src-reverie.",
  ].join("\n");
}

function isDirectoryEmpty(dir) {
  if (!existsSync(dir)) return true;
  return readdirSync(dir).length === 0;
}

function applyName(file, appName) {
  let text;
  try {
    text = readFileSync(file, "utf8");
  } catch {
    return;
  }
  if (!text.includes("__REVERIE_NAME__")) return;
  writeFileSync(file, text.replaceAll("__REVERIE_NAME__", appName), "utf8");
}

function applyNameRecursive(dir, appName) {
  for (const entry of readdirSync(dir)) {
    if (entry === "node_modules" || entry === ".git") continue;
    const path = join(dir, entry);
    if (statSync(path).isDirectory()) applyNameRecursive(path, appName);
    else applyName(path, appName);
  }
}

export function scaffold({ target, template, force }) {
  if (!target) throw new Error("a target directory is required");
  if (!TEMPLATE_NAMES.includes(template)) {
    throw new Error(`unknown template "${template}"; expected one of ${TEMPLATE_NAMES.join(", ")}`);
  }
  if (!existsSync(RUNTIME)) {
    throw new Error(`bundled runtime missing at ${RUNTIME}; run "npm run sync-runtime"`);
  }

  const appDir = resolve(process.cwd(), target);
  const appName = appDir.split(/[\\/]/).pop();

  if (existsSync(appDir) && !force && !isDirectoryEmpty(appDir)) {
    throw new Error(`target directory is not empty: ${appDir} (use --force to overwrite)`);
  }
  mkdirSync(appDir, { recursive: true });

  // Shared base files, then the frontend template.
  const baseDir = join(TEMPLATES, "_base");
  if (existsSync(baseDir)) cpSync(baseDir, appDir, { recursive: true });
  const templateDir = join(TEMPLATES, template);
  cpSync(templateDir, appDir, { recursive: true });
  applyNameRecursive(appDir, appName);

  // RingEcho runtime, C shim sources, and build scripts.
  cpSync(RUNTIME, join(appDir, "src-reverie"), { recursive: true });
  if (existsSync(SHIM)) cpSync(SHIM, join(appDir, "shim"), { recursive: true });
  if (existsSync(SCRIPTS)) {
    const dest = join(appDir, "scripts");
    mkdirSync(dest, { recursive: true });
    for (const name of ["fetch-webview2.ps1", "build-app.ps1"]) {
      const from = join(SCRIPTS, name);
      if (existsSync(from)) cpSync(from, join(dest, name));
    }
  }

  return { appDir, appName, template };
}

function main() {
  let options;
  try {
    options = parseArgs(process.argv.slice(2));
  } catch (error) {
    console.error(String(error.message ?? error));
    console.error(usage());
    process.exit(1);
  }

  if (options.help || !options.target) {
    console.log(usage());
    process.exit(options.target ? 0 : 1);
  }

  let result;
  try {
    result = scaffold(options);
  } catch (error) {
    console.error(`create-reverie: ${error.message ?? error}`);
    process.exit(1);
  }

  console.log(`Created ${result.template} app in ${result.appDir}`);
  console.log("");
  console.log("Next steps:");
  console.log(`  cd ${options.target}`);
  console.log(`  ${options.pm} install`);
  console.log(`  ${options.pm} run reverie:dev   # build and run the RingEcho runtime`);
  console.log(`  ${options.pm} run dev           # run the web frontend`);
}

if (import.meta.url === `file://${process.argv[1]}` || process.argv[1]?.endsWith("create-reverie.js")) {
  main();
}
