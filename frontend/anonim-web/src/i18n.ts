export type Locale = "en" | "ru";
export type TranslationKey = keyof typeof I18N.en;

export const I18N = {
  en: {
    appTitle: "Cipher Chat Client",
    language: "Language",
    langEn: "English",
    langRu: "Russian",
    loginTab: "Login",
    registerTab: "Register",
    username: "Username",
    password: "Password",
    e2eeKeys: "E2EE keys",
    e2eePlaceholder:
      "On register this app generates ECDH P-256 keys and uploads only public key",
    pleaseWait: "Please wait...",
    signIn: "Sign in",
    createAccount: "Create account",
    logout: "Logout",
    conversations: "Conversations",
    signedInAs: "Signed in as",
    authHint: "Please authenticate to open chat",
    guideTab: "Guide",
    chatTab: "Chat",
    profileTab: "Profile",
    peerUsername: "peer username",
    open: "Open",
    choosePeer: "Choose or add a peer to start chatting.",
    typeMessage: "type message",
    send: "Send",
    welcomeEyebrow: "welcome",
    guideHeroTitle:
      "Anonim is an encrypted messenger with browser-side key management",
    guideHeroText:
      "You can explore this guide without signing in. To start chatting, create an account, keep your private key on this device, and open a conversation with another user.",
    howToStart: "How to start",
    howToStart1: "Make sure backend is available at http://127.0.0.1:8090.",
    howToStart2:
      "Register a new account to generate your ECDH key pair in browser.",
    howToStart3: "Sign in and open chat with peer username.",
    howToStart4: "Type message and send. Ciphertext is stored on server.",
    messengerFeatures: "Messenger functionality",
    feature1: "Registration and login with token authentication.",
    feature2: "Peer-based dialogs with message polling every 3 seconds.",
    feature3: "Automatic decrypt for messages visible in active conversation.",
    feature4: "Local profile avatar customization in this browser.",
    encryptionBasics: "Encryption basics in this app",
    encryptionText:
      "Anonim uses ECDH on curve P-256 to derive a shared secret between two users. The shared secret becomes an AES-GCM 256 key used for message encryption and authentication.",
    encryption1:
      "Public key is uploaded to backend and shared for peer discovery.",
    encryption2:
      "Private key remains in localStorage on your current device only.",
    encryption3: "Each message uses a random nonce with AES-GCM.",
    encryption4:
      "Server stores ciphertext, nonce, metadata, but not plaintext.",
    avatarImage: "Avatar image (max 2 MB)",
    removeAvatar: "Remove avatar",
    saveProfile: "Save profile",
    reset: "Reset",
    avatarNote:
      "Avatar customization is local for this browser and does not change account login.",
    msgFillUsernamePassword: "Fill username and password",
    msgRegistrationSuccess:
      "Registration successful. Device key pair was generated.",
    msgRegistrationFailed: "Registration failed",
    msgWelcome: "Welcome",
    msgLoginFailed: "Login failed",
    msgEnterPeer: "Enter peer username",
    msgLoginFirst: "Login first",
    msgConnectedWith: "Connected with",
    msgCannotOpenChat: "Cannot open chat with this user",
    msgUnableToLoadMessages: "Unable to load messages",
    msgUnableToSendMessage: "Unable to send message",
    msgSessionCleared: "Session cleared",
    msgProfileSignInRequired: "Sign in first to update your profile",
    msgProfileUpdated: "Profile updated on this device",
    msgProfileReset: "Profile customization reset",
    msgChooseImageFile: "Choose an image file (png, jpg, webp, etc.)",
    msgImageTooLarge: "Image is too large. Maximum size is 2 MB",
    msgUnableToLoadImage: "Unable to load image",
    msgUnableToReadImage: "Unable to read selected image",
    msgE2eeUnavailable: "E2EE identity is unavailable in current browser",
    msgDecrypting: "[decrypting...]",
    msgDecryptFailed: "[unable to decrypt on this device]",
  },
  ru: {
    appTitle: "Клиент шифрованного чата",
    language: "Язык",
    langEn: "Английский",
    langRu: "Русский",
    loginTab: "Вход",
    registerTab: "Регистрация",
    username: "Логин",
    password: "Пароль",
    e2eeKeys: "Ключи E2EE",
    e2eePlaceholder:
      "При регистрации приложение генерирует ECDH P-256 ключи и отправляет на сервер только публичный ключ",
    pleaseWait: "Подождите...",
    signIn: "Войти",
    createAccount: "Создать аккаунт",
    logout: "Выйти",
    conversations: "Диалоги",
    signedInAs: "Вы вошли как",
    authHint: "Авторизуйтесь, чтобы открыть чат",
    guideTab: "Гайд",
    chatTab: "Чат",
    profileTab: "Профиль",
    peerUsername: "Логин собеседника",
    open: "Открыть",
    choosePeer: "Выберите или добавьте собеседника, чтобы начать чат.",
    typeMessage: "Введите сообщение",
    send: "Отправить",
    welcomeEyebrow: "Добро пожаловать",
    guideHeroTitle:
      "Anonim — зашифрованный мессенджер с управлением ключами в браузере",
    guideHeroText:
      "Этот гайд доступен без входа. Чтобы начать общение, создайте аккаунт, сохраните приватный ключ на этом устройстве и откройте диалог с другим пользователем.",
    howToStart: "Как начать",
    howToStart1:
      "Убедитесь, что backend доступен по адресу http://127.0.0.1:8090.",
    howToStart2:
      "Зарегистрируйте аккаунт, чтобы сгенерировать ECDH-пару ключей в браузере.",
    howToStart3: "Войдите и откройте чат по логину собеседника.",
    howToStart4:
      "Введите сообщение и отправьте. На сервере хранится только шифртекст.",
    messengerFeatures: "Функционал мессенджера",
    feature1: "Регистрация и вход с токен-аутентификацией.",
    feature2:
      "Диалоги по логину пользователя и обновление сообщений каждые 3 секунды.",
    feature3: "Автоматическая расшифровка сообщений в активном диалоге.",
    feature4: "Локальная настройка аватара профиля в этом браузере.",
    encryptionBasics: "Основы шифрования в приложении",
    encryptionText:
      "Anonim использует ECDH на кривой P-256 для вычисления общего секрета между двумя пользователями. Из него получается ключ AES-GCM 256 для шифрования и аутентификации сообщений.",
    encryption1:
      "Публичный ключ загружается на backend и используется для поиска ключа собеседника.",
    encryption2:
      "Приватный ключ хранится только в localStorage текущего устройства.",
    encryption3:
      "Каждое сообщение шифруется с новым случайным nonce в AES-GCM.",
    encryption4:
      "Сервер хранит шифртекст, nonce и метаданные, но не открытый текст.",
    avatarImage: "Изображение аватара (до 2 МБ)",
    removeAvatar: "Удалить аватар",
    saveProfile: "Сохранить профиль",
    reset: "Сброс",
    avatarNote:
      "Настройка аватара локальная для этого браузера и не меняет логин аккаунта.",
    msgFillUsernamePassword: "Заполните логин и пароль",
    msgRegistrationSuccess:
      "Регистрация успешна. Пара ключей устройства создана.",
    msgRegistrationFailed: "Ошибка регистрации",
    msgWelcome: "Добро пожаловать",
    msgLoginFailed: "Ошибка входа",
    msgEnterPeer: "Введите логин собеседника",
    msgLoginFirst: "Сначала выполните вход",
    msgConnectedWith: "Подключено к",
    msgCannotOpenChat: "Невозможно открыть чат с этим пользователем",
    msgUnableToLoadMessages: "Не удалось загрузить сообщения",
    msgUnableToSendMessage: "Не удалось отправить сообщение",
    msgSessionCleared: "Сессия очищена",
    msgProfileSignInRequired: "Сначала выполните вход, чтобы обновить профиль",
    msgProfileUpdated: "Профиль обновлен на этом устройстве",
    msgProfileReset: "Настройки профиля сброшены",
    msgChooseImageFile: "Выберите файл изображения (png, jpg, webp и т.д.)",
    msgImageTooLarge: "Изображение слишком большое. Максимальный размер 2 МБ",
    msgUnableToLoadImage: "Не удалось загрузить изображение",
    msgUnableToReadImage: "Не удалось прочитать выбранное изображение",
    msgE2eeUnavailable: "E2EE-ключи недоступны в текущем браузере",
    msgDecrypting: "[Расшифровка...]",
    msgDecryptFailed: "[не удалось расшифровать на этом устройстве]",
  },
} as const;

export function createTranslator(locale: Locale) {
  return function t(key: TranslationKey): string {
    return I18N[locale][key];
  };
}
