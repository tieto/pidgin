;;
;;  lithuanian.nsh
;;
;;  Lithuanian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define PIDGIN_IS_RUNNING			"Ðiuo metu Pidgin yra paleistas. Uþdarykite ðià programà ir pabandykite ið naujo."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Toliau >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) yra iðleistas GNU bendrosios vieðosios licenzijos (GPL) sàlygomis.  Licenzija èia yra pateikta tik susipaþinimo tikslams. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin pokalbiø kliento programa (bûtina)"
!define GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (bûtina, jeigu nëra)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"darbalaukyje"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"pradiniame meniu"
!define PIDGIN_SECTION_DESCRIPTION		"Pagrindiniai Pidgin failai ir DLL bibliotekos"
!define GTK_SECTION_DESCRIPTION		"Daugiaplatforminis vartotojo sàsajos priemoniø komplektas, naudojamas Pidgin."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin paleidimo nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Sukurti nuorodà á Pidgin darbastalyje."
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Sukurti pradinio meniu áraðà, skirtà Pidgin."

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Aplankyti Pidgin tinklalapá"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Nepavyko iðdiegti anksèiau ádiegtos Pidgin versijos.  Nauja versija bus ádiegta neiðdiegus senosios."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI doroklës"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Iðdiegimo programa nerado Pidgin registro áraðø.$\rTikëtina, kad progama buvo ádiegta kito naudotojo."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Jûs neturite teisiø iðdiegti ðios programos."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Raðybos tikrinimo palaikymas"
!define PIDGIN_SPELLCHECK_ERROR		"Raðybos tikrinimo palaikymo diegimo klaida"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Raðybos tikrinimo palaikymas.  (Diegimui bûtina interneto jungtis)"

