export type AuthMode = "login" | "register";

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

export type PublicKeyResponse = {
  username: string;
  publicKey: string;
};

export const STORAGE_KEYS = {
  token: "anonim.token",
  username: "anonim.username",
  privateKeyPrefix: "anonim.privateKey.",
  publicKeyPrefix: "anonim.publicKey.",
  avatarPrefix: "anonim.avatar.",
  locale: "anonim.locale",
} as const;
