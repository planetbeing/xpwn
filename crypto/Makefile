all:	aes patch-kernel-crypto

aes:	aes.c
	gcc aes.c -lIOKit -o aes
	ldid -S aes

patch-kernel-crypto:	patch-kernel-crypto.c
	gcc patch-kernel-crypto.c -o patch-kernel-crypto
	ldid -S patch-kernel-crypto

clean:
	rm aes patch-kernel-crypto

