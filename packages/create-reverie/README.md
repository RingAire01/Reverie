# create-reverie

Scaffold a Reverie desktop app: a RingEcho runtime in `src-reverie/` plus a web
frontend in `src/`, modelled on Tauri's `src-tauri/` split so it fits JS/TS
tooling (Vite, React, Vue, Next.js).

Published under the `ringaire` npm organization.

```sh
pnpm create @ringaire/reverie my-app --template react
# or from this repo:
node packages/create-reverie/bin/create-reverie.js my-app --template react
```

The generated app uses pnpm by default; override with `--pm npm|pnpm|bun`.

## Publish

```sh
pnpm --filter @ringaire/create-reverie publish --access public
```

## Templates

`vanilla`, `react`, `vue`, `next`.

The `react`, `vue` and `next` templates are TypeScript. Every app gets a typed
bridge at `src/reverie.ts`:

```ts
import { invoke, onMessage, isReverie } from "./reverie.ts";

invoke("greet", "world");        // -> native hears "greet:world"
const off = onMessage((msg) => console.log(msg)); // native -> page
```

## Layout produced

```
my-app/
  package.json          frontend + "reverie:dev" scripts
  src/                  web frontend
  src-reverie/          RingEcho runtime (.reo)
    main.reo            native entry
    reverie.reo         public facade
    reverie/            abi + window + loop modules
```

## Maintenance

`runtime/` is a copy of the repository's authoritative runtime (`/src`). Refresh
it before publishing:

```sh
npm run sync-runtime
```
