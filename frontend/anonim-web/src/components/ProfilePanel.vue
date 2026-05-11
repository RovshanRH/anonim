<script setup lang="ts">
type Translator = (...args: any[]) => string;

defineProps<{
  t: Translator;
  isAuthenticated: boolean;
  profileAvatarDataUrl: string;
  visibleName: string;
  visibleInitial: string;
  currentUser: string;
  avatarInputError: string;
}>();

const emit = defineEmits<{
  (event: "avatar-input", value: Event): void;
  (event: "clear-avatar"): void;
  (event: "save-profile"): void;
  (event: "reset-profile"): void;
}>();
</script>

<template>
  <section class="profile-panel">
    <div class="profile-preview">
      <img
        v-if="profileAvatarDataUrl"
        :src="profileAvatarDataUrl"
        class="profile-avatar"
        alt="profile avatar"
      />
      <div v-else class="profile-avatar profile-avatar--placeholder">
        {{ visibleInitial }}
      </div>
      <div>
        <h3>{{ visibleName }}</h3>
        <p>@{{ currentUser || "guest" }}</p>
      </div>
    </div>

    <label class="field">
      <span>{{ t("avatarImage") }}</span>
      <input
        type="file"
        accept="image/*"
        :disabled="!isAuthenticated"
        @change="emit('avatar-input', $event)"
      />
    </label>

    <div class="profile-actions">
      <button
        class="btn btn--ghost"
        :disabled="!profileAvatarDataUrl || !isAuthenticated"
        @click="emit('clear-avatar')"
      >
        {{ t("removeAvatar") }}
      </button>
      <button
        class="btn btn--primary"
        :disabled="!isAuthenticated"
        @click="emit('save-profile')"
      >
        {{ t("saveProfile") }}
      </button>
      <button
        class="btn btn--ghost"
        :disabled="!isAuthenticated"
        @click="emit('reset-profile')"
      >
        {{ t("reset") }}
      </button>
    </div>

    <p class="profile-note">{{ t("avatarNote") }}</p>
    <p v-if="avatarInputError" class="flash flash--error">
      {{ avatarInputError }}
    </p>
  </section>
</template>
