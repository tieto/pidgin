CERTIFICATES = \
		CAcert_Root.pem \
		CAcert_Class3.pem \
		Equifax_Secure_CA.pem \
		GTE_CyberTrust_Global_Root.pem \
		Microsoft_Secure_Server_Authority.pem \
		StartCom_Free_SSL_CA.pem \
		Verisign_RSA_Secure_Server_CA.pem \
		Verisign_Class3_Primary_CA.pem

if INSTALL_SSL_CERTIFICATES
cacertsdir =	$(datadir)/purple/ca-certs
cacerts_DATA =	$(CERTIFICATES)
endif

EXTRA_DIST =	\
		Makefile.mingw \
		$(CERTIFICATES)

