This document is written more for myself than for the community. But if
somebody else someday has to do a release (because, I dunno, I gave up
DNS for NetBIOS), this could be helpful. :)

## Merge!

If there's a branch to merge, simply run:

    git checkout master
    git pull
    git merge otherbranch
    git push

Super simple!

## Make sure the version number and docs are up to date

Both client/dnscat.c and server/dnscat2.rb contain a version number at
the top. Perhaps I should store it in a single place and include it
automatically, but at the moment it's not.

Things in the tools/ directory also have their own independent version
numbers. They should be updated if and only if the tool itself was
updated in some way.

Make sure other documentation is up to date, such as usage and output.

And finally, make sure docs/changelog.md is up to date.

## Make sure it compiles on all platforms

Compile and give it a cursory test on all supported platforms:

* Linux (32- and 64-bit)
* FreeBSD
* Mac OS X
* Windows (via Visual Studio)

Be sure to compile in release mode, "make release", which enables
optimizations and disables some debug flags.

On Windows, you will also need to set the build profile to "Release".

It's especially important to make sure that Windows works, because
Visual Studio is so different from the rest of the platforms.

## Compile and upload the distribution files

Release versions on Linux can be compiled using:

    make release

Source distros can be packaged using:

    make source_release

It even zips them for you! They're put into the dist/ folder.

Releases on other platforms (like Windows) require some manual work at
the moment. Please try to follow my naming scheme:

dnscat2-v0.04-client-source.tar.bz2
dnscat2-v0.04-client-source.zip
dnscat2-v0.04-client-win32.zip
dnscat2-v0.04-client-x64.tar.bz2
dnscat2-v0.04-client-x86.tar.bz2
dnscat2-v0.04-server.tar.bz2
dnscat2-v0.04-server.zip

For binaries, the binaries in the archive should be simply "dnscat" - no
paths or anything like that.

FWIW, I don't provide a zip of the client and server source together
because that's exactly just what you get on github. :)

## Sign and upload the release files

First, create signatures for all the files:

    rm *.sig; for i in *; do gpg --armor --detach-sig --output $i.sig $i; done

Then upload them to https://downloads.skullsecurity.org/dnscat2/

## Create and push a signed tag:

Create a signed tag with a comment like this one:

    git tag -s "v0.02" -m "Beta release 2"

Then push it upstream:

    git push origin v0.02

Once that's done, to publish the release:

* Add release notes on https://github.com/iagox86/dnscat2/tags
* Publish it on: https://github.com/iagox86/dnscat2/releases

## In case you need to delete a tag

If you screw up, which I always do, here's the process to delete a tag:

    git tag -d v0.02
    git push origin :refs/tags/v0.02
