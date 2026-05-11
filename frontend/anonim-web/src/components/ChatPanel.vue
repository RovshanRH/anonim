<script setup lang="ts">
// Панель чата: выбор/подключение к собеседнику, показ зашифрованных сообщений и отправка.
type Translator = (...args: any[]) => string;

type ApiMessage = {
  id: number;
  from: string;
  to: string;
  messageType?: "text";
  ciphertext: string;
  nonce: string;
  encryption: string;
  timestamp: string;
};

defineProps<{
  t: Translator;
  isAuthenticated: boolean;
  isBusy: boolean;
  currentUser: string;
  peerInput: string;
  peers: string[];
  activePeer: string;
  draftMessage: string;
  messages: ApiMessage[];
  getMessageText: (message: ApiMessage) => string;
}>();

const emit = defineEmits<{
  (event: "update:peer-input", value: string): void;
  (event: "update:active-peer", value: string): void;
  (event: "update:draft-message", value: string): void;
  (event: "attach-peer"): void;
  (event: "load-messages"): void;
  (event: "send-message"): void;
}>();

function selectPeer(peer: string): void {
  emit("update:active-peer", peer);
  emit("load-messages");
}
</script>

<template>
  <!-- Основная разметка панели чата -->
  <template v-if="true">
    <div class="peer-input">
      <input
        :value="peerInput"
        :disabled="!isAuthenticated"
        :placeholder="t('peerUsername')"
        @input="
          emit('update:peer-input', ($event.target as HTMLInputElement).value)
        "
        @keydown.enter.prevent="emit('attach-peer')"
      />
      <button
        class="btn btn--accent"
        :disabled="!isAuthenticated || isBusy"
        @click="emit('attach-peer')"
      >
        {{ t("open") }}
      </button>
    </div>

    <div class="peers">
      <button
        v-for="peer in peers"
        :key="peer"
        class="peer-chip"
        :class="{ active: peer === activePeer }"
        @click="selectPeer(peer)"
      >
        {{ peer }}
      </button>
    </div>

    <div v-if="activePeer" class="messages">
      <article
        v-for="message in messages"
        :key="message.id"
        class="bubble"
        :class="{ 'bubble--mine': message.from === currentUser }"
      >
        <header>
          <span>{{ message.from }}</span>
          <time>{{ message.timestamp }}</time>
        </header>
        <p>{{ getMessageText(message) }}</p>
        <small>{{ message.encryption }} • nonce {{ message.nonce }}</small>
      </article>
    </div>

    <p v-else class="empty">{{ t("choosePeer") }}</p>

    <form class="composer" @submit.prevent="emit('send-message')">
      <textarea
        :value="draftMessage"
        :disabled="!activePeer || !isAuthenticated"
        rows="3"
        :placeholder="t('typeMessage')"
        @input="
          emit(
            'update:draft-message',
            ($event.target as HTMLTextAreaElement).value,
          )
        "
      ></textarea>
      <div class="composer-actions">
        <button
          class="btn btn--primary"
          :disabled="!draftMessage.trim() || !activePeer || isBusy"
        >
          {{ t("send") }}
        </button>
      </div>
    </form>
  </template>
</template>
