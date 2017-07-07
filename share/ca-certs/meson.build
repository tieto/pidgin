CERTIFICATES = [
	'CAcert_Root.pem',
	'CAcert_Class3.pem',
	'mozilla.pem'
]

EXTRA_CERTS = [
#	'filename.pem'
]

if INSTALL_SSL_CERTIFICATES
	cacerts_DATA = CERTIFICATES + EXTRA_CERTS
else
	cacerts_DATA = EXTRA_CERTS
endif

install_data(sources : cacerts_DATA,
             install_dir : get_option('datadir') + '/purple/ca-certs')
