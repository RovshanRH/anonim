// Точка входа приложения: установка локали и монтирование Vue-приложения
import { createApp } from 'vue'
import App from './App.vue'
import './style.css'
import { I18N, type Locale } from './i18n'
import { STORAGE_KEYS } from './appShared'

// Получаем локаль из localStorage или по умолчанию 'en'
function getInitialLocale(): Locale {
	const persistedLocale = localStorage.getItem(STORAGE_KEYS.locale)
	return persistedLocale === 'ru' ? 'ru' : 'en'
}

const locale = getInitialLocale()

document.documentElement.lang = locale
document.title = I18N[locale].appTitle

createApp(App).mount('#app')
