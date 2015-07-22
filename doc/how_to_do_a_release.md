NOTE: This is just scratch notes right now, I'll turn it into a document
in the near future!


* 
* Update the version number in the client and server
* Make sure -DMEMORYTEST is off
* Make sure it compiles and runs on Windows (on release mode) + Linux

* Create a signed tag:

git tag -s "v0.02" -m "Beta release 2"

* Push the tag:

git push origin v0.02

* Add release notes: https://github.com/iagox86/dnscat2/tags
* Publish the release: https://github.com/iagox86/dnscat2/releases

* Compile the client on 32- and 64-bit linux
* dnscat-linux-x86 dnscat-linux-x64 dnscat-windows-

* Compile on Windows on *release* mode
* dnscat-win32

Zip the files:

The Linux clients should be made into a .tar.bz2 without a folder

The server should be .tar.bz2 and .zip in dnscat2-server/

mv server dnscat2-server
tar -cvvjf dnscat2-v0.02beta-server.tar.bz2 dnscat2-server/
zip -r dnscat2-v0.02beta-server.zip dnscat2-server/
mv dnscat2-server server


The client source should be .tar.bz2 and .zip in dnscat2-client-source/

mv client dnscat2-client
tar -cvvjf dnscat2-v0.02beta-client-source.tar.bz2 dnscat2-client-source/
zip -r dnscat2-v0.02beta-client-source.zip dnscat2-client-source/
mv dnscat2-client client

The filenames should be something like:

dnscat2-v0.02beta-client-win32.zip
dnscat2-v0.02beta-client-x64.tar.bz2
dnscat2-v0.02beta-client-x86.tar.bz2
dnscat2-v0.02beta-client-source.tar.bz2
dnscat2-v0.02beta-client-source.zip
dnscat2-v0.02beta-server.tar.bz2
dnscat2-v0.02beta-server.zip

Sign them:

rm *.sig; for i in *; do gpg --armor --detach-sig --output $i.sig $i; done



Deleting a tag:

git tag -d 12345
git push origin :refs/tags/12345


