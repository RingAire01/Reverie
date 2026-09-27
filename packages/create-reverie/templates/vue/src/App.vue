<script setup lang="ts">
import { onMounted, onUnmounted, ref } from "vue";
import { invoke, onMessage, isReverie } from "./reverie.ts";

const fromNative = ref("");
const running = ref(false);
let off: () => void = () => {};

onMounted(() => {
  running.value = isReverie();
  off = onMessage((message) => (fromNative.value = message));
  invoke("greet", "world");
});
onUnmounted(() => off());
</script>

<template>
  <main style="font-family: system-ui, sans-serif; padding: 24px">
    <h1>__REVERIE_NAME__</h1>
    <p>runtime: {{ running ? "Reverie" : "browser (no native bridge)" }}</p>
    <p>from native: <b>{{ fromNative || "(waiting)" }}</b></p>
    <button @click="invoke('ping')">ping</button>
  </main>
</template>
