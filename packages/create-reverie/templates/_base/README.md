# __REVERIE_NAME__

A Reverie desktop app.

- `src/` — the web frontend (edit freely).
- `src-reverie/` — the RingEcho runtime (`.reo`), built with `rev`.

## Scripts

```sh
npm install
npm run dev          # web frontend (Vite / Next)
npm run reverie:dev  # build and run the RingEcho runtime
```

Both halves are developed independently; they are joined by the Reverie IPC
bridge (see the Reverie project).
