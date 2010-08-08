CERTIFICATES = \
		America_Online_Root_Certification_Authority_1.pem \
		CAcert_Root.pem \
		CAcert_Class3.pem \
		Entrust.net_Secure_Server_CA.pem \
		Equifax_Secure_CA.pem \
		Equifax_Secure_Global_eBusiness_CA-1.pem \
		GTE_CyberTrust_Global_Root.pem \
		StartCom_Certification_Authority.pem \
		StartCom_Free_SSL_CA.pem \
		Thawte_Premium_Server_CA.pem \
		ValiCert_Class_2_VA.pem \
		Verisign_RSA_Secure_Server_CA.pem \
		Verisign_Class3_Primary_CA.pem \
		VeriSign_Class_3_Public_Primary_Certification_Authority_-_G5.pem \
		VeriSign_Class_3_Public_Primary_Certification_Authority_-_G5_2.pem

EXTRA_CERTS = \
		AOL_Member_CA.pem \
		Microsoft_Internet_Authority.pem \
		Microsoft_Secure_Server_Authority.pem \
		VeriSign_Class3_Extended_Validation_CA.pem \
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
