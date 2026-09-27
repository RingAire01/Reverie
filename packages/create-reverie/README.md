# create-reverie

Scaffold a Reverie desktop app: a RingEcho runtime in `src-reverie/` plus a web
frontend in `src/`, modelled on Tauri's `src-tauri/` split so it fits JS/TS
tooling (Vite, React, Vue, Next.js).

Published under the `ringaire` npm organization.

```sh
npm create @ringaire/reverie@latest my-app -- --template react
# or from this repo:
node packages/create-reverie/bin/create-reverie.js my-app --template react
```

## Templates

`vanilla`, `react`, `vue`, `next`.

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
