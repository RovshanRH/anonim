<script setup lang="ts">
defineOptions({ name: "GuideSection" });

type Translator = (...args: any[]) => string;

defineProps<{
  t: Translator;
}>();
</script>

<template>
  <section class="landing-panel">
    <div class="landing-hero">
      <p class="eyebrow">{{ t("welcomeEyebrow") }}</p>
      <h3>{{ t("guideHeroTitle") }}</h3>
      <p>{{ t("guideHeroText") }}</p>
    </div>

    <div class="landing-grid">
      <article class="landing-card">
        <h4>{{ t("howToStart") }}</h4>
        <ol>
          <li>{{ t("howToStart1") }}</li>
          <li>{{ t("howToStart2") }}</li>
          <li>{{ t("howToStart3") }}</li>
          <li>{{ t("howToStart4") }}</li>
        </ol>
      </article>

      <article class="landing-card">
        <h4>{{ t("messengerFeatures") }}</h4>
        <ul>
          <li>{{ t("feature1") }}</li>
          <li>{{ t("feature2") }}</li>
          <li>{{ t("feature3") }}</li>
          <li>{{ t("feature4") }}</li>
        </ul>
      </article>

      <article class="landing-card landing-card--wide">
        <h4>{{ t("encryptionBasics") }}</h4>
        <p>{{ t("encryptionText") }}</p>
        <ul>
          <li>{{ t("encryption1") }}</li>
          <li>{{ t("encryption2") }}</li>
          <li>{{ t("encryption3") }}</li>
          <li>{{ t("encryption4") }}</li>
        </ul>
      </article>
    </div>
  </section>
</template>
