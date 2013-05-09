/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

/* aes.c */
PurpleCipherOps * purple_aes_cipher_get_ops(void);

/* des.c */
PurpleCipherOps * purple_des_cipher_get_ops(void);
PurpleCipherOps * purple_des3_cipher_get_ops(void);

/* gchecksum.c */
PurpleCipherOps * purple_md5_cipher_get_ops(void);
PurpleCipherOps * purple_sha1_cipher_get_ops(void);
PurpleCipherOps * purple_sha256_cipher_get_ops(void);

/* hmac.c */
PurpleCipherOps * purple_hmac_cipher_get_ops(void);

/* md4.c */
PurpleCipherOps * purple_md4_cipher_get_ops(void);

/* pbkdf2.c */
PurpleCipherOps * purple_pbkdf2_cipher_get_ops(void);

/* rc4.c */
PurpleCipherOps * purple_rc4_cipher_get_ops(void);

static inline void purple_ciphers_register_all(void)
{
#if defined(HAVE_GNUTLS) || defined(HAVE_NSS)
	purple_ciphers_register_cipher("aes", purple_aes_cipher_get_ops());
#endif

	purple_ciphers_register_cipher("des", purple_des_cipher_get_ops());
	purple_ciphers_register_cipher("des3", purple_des3_cipher_get_ops());

	purple_ciphers_register_cipher("md5", purple_md5_cipher_get_ops());
	purple_ciphers_register_cipher("sha1", purple_sha1_cipher_get_ops());
	purple_ciphers_register_cipher("sha256", purple_sha256_cipher_get_ops());

	purple_ciphers_register_cipher("hmac", purple_hmac_cipher_get_ops());

	purple_ciphers_register_cipher("md4", purple_md4_cipher_get_ops());

	purple_ciphers_register_cipher("pbkdf2", purple_pbkdf2_cipher_get_ops());

	purple_ciphers_register_cipher("rc4", purple_rc4_cipher_get_ops());
}
