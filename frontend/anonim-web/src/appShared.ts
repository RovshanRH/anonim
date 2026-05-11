// Тип режима аутентификации в UI
export type AuthMode = "login" | "register";

// Тип сообщения, получаемого от API (зашифрованное)
export type ApiMessage = {
  id: number;
  from: string;
  to: string;
  messageType?: "text";
  ciphertext: string;
  nonce: string;
  encryption: string;
  timestamp: string;
};

// Ответ сервера с публичным ключом пользователя
export type PublicKeyResponse = {
  username: string;
  publicKey: string;
};

// Ключи для localStorage, используемые приложением
export const STORAGE_KEYS = {
  token: "anonim.token",
  username: "anonim.username",
  privateKeyPrefix: "anonim.privateKey.",
  publicKeyPrefix: "anonim.publicKey.",
  avatarPrefix: "anonim.avatar.",
  locale: "anonim.locale",
} as const;
