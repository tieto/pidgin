;;
;;  french.nsh
;;
;;  French language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Eric Boumaour <zongo@nekeme.net>, 2003.
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_FRENCH} "Les bibliothèques de l'environnement GTK+ ne sont pas installées ou nécessitent une mise à jour.$\rVeuillez installer les bibliothèques GTK+ v${GTK_VERSION} ou plus récentes."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_FRENCH} "Gaim messagerie instantanée (obligatoire)"
LangString GTK_SECTION_TITLE				${LANG_FRENCH} "Bibliothèques GTK+ (obligatoire)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_FRENCH} "Thèmes GTK+"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_FRENCH} "Pas de thème"
LangString GTK_WIMP_SECTION_TITLE			${LANG_FRENCH} "Thème Wimp"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_FRENCH} "Thème Bluecurve"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_FRENCH} "Thème Light House Blue"
LangString GAIM_SECTION_DESCRIPTION			${LANG_FRENCH} "Fichiers et DLLs de base de Gaim"
LangString GTK_SECTION_DESCRIPTION			${LANG_FRENCH} "A multi-platform GUI toolkit, used by Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_FRENCH} "Les thèmes GTK+ permettent de changer l'aspect des applications GTK+."
LangString GTK_NO_THEME_DESC				${LANG_FRENCH} "Ne pas installer de thème GTK+"
LangString GTK_WIMP_THEME_DESC			${LANG_FRENCH} "GTK-Wimp (imitateur de Windows) est un thème de GTK+ qui se fond dans l'environnement graphique de Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_FRENCH} "Thème Bluecurve"
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_FRENCH} "Thème Lighthouseblue"

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_FRENCH} "Emplacement de l'installation"
LangString GTK_PAGE_SUBTITLE				${LANG_FRENCH} "Choisissez le répertoire dans lequel installer GTK+"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_FRENCH} "GTK+ sera installé dans le répertoire suivant"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_FRENCH} "Pour l'installer dans un répertoire différent, cliquez Parcourir et choisissez un autre répertoire. Cliquez sur Suivant pour continuer."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_FRENCH} "GTK+ sera mis à jour dans le répertoire suivant"
LangString GTK_UPGRADE_PROMPT				${LANG_FRENCH} "Une ancienne version des bibliothèques GTK+ a été trouvée. Voulez-vous la mettre à jour ?$\rNote : Gaim peut ne pas fonctionner sans cela."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_FRENCH} "Gaim (supprimer uniquement)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_FRENCH} "L'ancien répertoire de Gaim va être supprimé. Voulez-vous continuer ?$\r$\rNote : Tous les plugins non standards que vous avez installés seront aussi supprimés.$\rLes configurations des utilisateurs de Gaim ne sont pas touchés."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_FRENCH} "Le répertoire d'installation existe déjà. Son contenu sera effacé.$\rVoulez-vous continuer ?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_FRENCH} "Erreur lors de l'installation des bibliothèques GTK+"
LangString GTK_BAD_INSTALL_PATH			${LANG_FRENCH} "Le répertoire d'installation ne peut pas être créé ou n'est pas accessible."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_FRENCH} "Vous n'avez pas les permissions pour installer un thème GTK+"

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_FRENCH} "Les clefs de Gaim n'ont pas été trouvées dans la base de registres.$\rL'application a peut-être été installée par un utilisateur différent."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_FRENCH} "Vous n'avez pas les permissions pour supprimer cette application."

;; vim:syn=winbatch:encoding=8bit-cp1252:
