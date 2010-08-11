CERTIFICATES = \
		America_Online_Root_Certification_Authority_1.pem \
		CAcert_Root.pem \
		CAcert_Class3.pem \
		Equifax_Secure_CA.pem \
		GTE_CyberTrust_Global_Root.pem \
		StartCom_Free_SSL_CA.pem \
		Verisign_RSA_Secure_Server_CA.pem \
		Verisign_Class3_Primary_CA.pem \
		VeriSign_Class_3_Public_Primary_Certification_Authority_-_G5.pem

EXTRA_CERTS = \
		AOL_Member_CA.pem \
		Microsoft_Internet_Authority.pem \
		Microsoft_Secure_Server_Authority.pem \
		VeriSign_International_Server_Class_3_CA.pem


cacertsdir =	$(datadir)/purple/ca-certs

if INSTALL_SSL_CERTIFICATES
cacerts_DATA = \
	$(CERTIFICATES) \
	$(EXTRA_CERTS)
else
cacerts_DATA = $(EXTRA_CERTS)
endif

EXTRA_DIST =	\
		Makefile.mingw \
		$(CERTIFICATES) \
		$(EXTRA_CERTS)
