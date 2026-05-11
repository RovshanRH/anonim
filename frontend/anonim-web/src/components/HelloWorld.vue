<script setup lang="ts">
defineOptions({ name: "AuthPanel" });

type Translator = (...args: any[]) => string;

defineProps<{
  t: Translator;
  locale: "en" | "ru";
  authMode: "login" | "register";
  username: string;
  password: string;
  isBusy: boolean;
  isAuthenticated: boolean;
  infoMessage: string;
  errorMessage: string;
}>();

const emit = defineEmits<{
  (event: "update:locale", value: "en" | "ru"): void;
  (event: "update:auth-mode", value: "login" | "register"): void;
  (event: "update:username", value: string): void;
  (event: "update:password", value: string): void;
  (event: "submit"): void;
  (event: "logout"): void;
}>();
</script>

<template>
  <section class="panel panel--auth">
    <header class="panel__header">
      <p class="eyebrow">anonim</p>
      <h1>{{ t("appTitle") }}</h1>
    </header>

    <label class="field">
      <span>{{ t("language") }}</span>
      <select
        :value="locale"
        @change="
          emit(
            'update:locale',
            ($event.target as HTMLSelectElement).value as 'en' | 'ru',
          )
        "
      >
        <option value="en">{{ t("langEn") }}</option>
        <option value="ru">{{ t("langRu") }}</option>
      </select>
    </label>

    <div class="tabs">
      <button
        :class="{ active: authMode === 'login' }"
        @click="emit('update:auth-mode', 'login')"
      >
        {{ t("loginTab") }}
      </button>
      <button
        :class="{ active: authMode === 'register' }"
        @click="emit('update:auth-mode', 'register')"
      >
        {{ t("registerTab") }}
      </button>
    </div>

    <label class="field">
      <span>{{ t("username") }}</span>
      <input
        :value="username"
        autocomplete="username"
        placeholder="alice"
        @input="
          emit('update:username', ($event.target as HTMLInputElement).value)
        "
      />
    </label>

    <label class="field">
      <span>{{ t("password") }}</span>
      <input
        :value="password"
        type="password"
        autocomplete="current-password"
        placeholder="********"
        @input="
          emit('update:password', ($event.target as HTMLInputElement).value)
        "
      />
    </label>

    <label v-if="authMode === 'register'" class="field">
      <span>{{ t("e2eeKeys") }}</span>
      <textarea
        rows="4"
        disabled
        :placeholder="t('e2eePlaceholder')"
      ></textarea>
    </label>

    <button class="btn btn--primary" :disabled="isBusy" @click="emit('submit')">
      {{
        isBusy
          ? t("pleaseWait")
          : authMode === "login"
            ? t("signIn")
            : t("createAccount")
      }}
    </button>

    <button
      v-if="isAuthenticated"
      class="btn btn--ghost"
      @click="emit('logout')"
    >
      {{ t("logout") }}
    </button>

    <p v-if="infoMessage" class="flash flash--info">{{ infoMessage }}</p>
    <p v-if="errorMessage" class="flash flash--error">{{ errorMessage }}</p>
  </section>
</template>
