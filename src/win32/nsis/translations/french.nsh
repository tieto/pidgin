;;
;;  french.nsh
;;
;;  French language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Eric Boumaour <zongo@nekeme.net>, 2003.
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Les bibliothèques de l'environnement GTK+ ne sont pas installées ou nécessitent une mise à jour.$\rVeuillez installer les bibliothèques GTK+ v${GTK_VERSION} ou plus récentes."

; License Page
!define GAIM_LICENSE_BUTTON			"Suivant >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) est disponible sous licence GPL. Le texte de licence suivant est fourni uniquement à titre informatif. $_CLICK" 

; Components Page
!define GAIM_SECTION_TITLE			"Gaim messagerie instantanée (obligatoire)"
!define GTK_SECTION_TITLE			"Bibliothèques GTK+ (obligatoire)"
!define GTK_THEMES_SECTION_TITLE		"Thèmes GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Pas de thème"
!define GTK_WIMP_SECTION_TITLE		"Thème Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Thème Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Thème Light House Blue"
!define GAIM_SECTION_DESCRIPTION		"Fichiers et DLLs de base de Gaim"
!define GTK_SECTION_DESCRIPTION		"A multi-platform GUI toolkit, used by Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Les thèmes GTK+ permettent de changer l'aspect des applications GTK+."
!define GTK_NO_THEME_DESC			"Ne pas installer de thème GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (imitateur de Windows) est un thème de GTK+ qui se fond dans l'environnement graphique de Windows."
!define GTK_BLUECURVE_THEME_DESC		"Thème Bluecurve"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Thème Lighthouseblue"

; GTK+ Dir Selector Page
!define GTK_UPGRADE_PROMPT			"Une ancienne version des bibliothèques GTK+ a été trouvée. Voulez-vous la mettre à jour ?$\rNote : Gaim peut ne pas fonctionner sans cela."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visitez la page web de Gaim Windows" 

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (supprimer uniquement)"
!define GAIM_PROMPT_WIPEOUT			"L'ancien répertoire de Gaim va être supprimé. Voulez-vous continuer ?$\r$\rNote : Tous les plugins non standards que vous avez installés seront aussi supprimés.$\rLes configurations des utilisateurs de Gaim ne sont pas touchés."
!define GAIM_PROMPT_DIR_EXISTS		"Le répertoire d'installation existe déjà. Son contenu sera effacé.$\rVoulez-vous continuer ?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erreur lors de l'installation des bibliothèques GTK+"
!define GTK_BAD_INSTALL_PATH			"Le répertoire d'installation ne peut pas être créé ou n'est pas accessible."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Vous n'avez pas les permissions pour installer un thème GTK+"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Les clefs de Gaim n'ont pas été trouvées dans la base de registres.$\rL'application a peut-être été installée par un utilisateur différent."
!define un.GAIM_UNINSTALL_ERROR_2         "Vous n'avez pas les permissions pour supprimer cette application."

