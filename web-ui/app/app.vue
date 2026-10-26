<script setup lang="ts">
import type { ToasterProps } from '@nuxt/ui/runtime/components/Toaster.d.vue.js';


const toaster = { position: 'top-center' } as ToasterProps
</script>
<template>
  <UApp :toaster="toaster">
    <NuxtLayout>
      <NuxtPage />
    </NuxtLayout>
  </UApp>
</template>