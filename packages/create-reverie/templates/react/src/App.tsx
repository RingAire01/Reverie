import { useEffect, useState } from "react";
import { invoke, onMessage, isReverie } from "./reverie.ts";

export default function App() {
  const [fromNative, setFromNative] = useState("");
  const [running, setRunning] = useState(false);

  useEffect(() => {
    setRunning(isReverie());
    const off = onMessage(setFromNative);
    invoke("greet", "world");
    return off;
  }, []);

  return (
    <main style={{ fontFamily: "system-ui, sans-serif", padding: 24 }}>
      <h1>__REVERIE_NAME__</h1>
      <p>runtime: {running ? "Reverie" : "browser (no native bridge)"}</p>
      <p>from native: <b>{fromNative || "(waiting)"}</b></p>
      <button onClick={() => invoke("ping")}>ping</button>
    </main>
  );
}
