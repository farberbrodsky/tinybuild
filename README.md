a# tinybuild
Tiny sudoless containers for easy reproducible builds

## Usage

The tinybuild binary creates a folder .tinybuild that is in the same folder as itself.

To load a base image, use: ```tinybuild untar-img x.tar image_name```. The untarred image is stored in .tinybuild/image_name and an md5 hash of the tar is stored in .tinybuild/md5-image_name.

Once you have a base image, you can create and use images with the following environment variables (in order of usage):
- **IMGCOPYxx**: Copy this file to the image, before building. e.g. ```IMGCOPY0="/etc/resolv.conf:/etc/resolv.conf"```

You can have as many of these as you want, and they are taken in lexical order - so IMGCOPY0 goes before IMGCOPY1, which is the recommended way to do this. You do have to pad with zeroes, e.g. 01 and 03 and 12, if you have more than 10 copies to do (very unlikely). You can even omit that and just write "IMGCOPY".

- **INSTALLxx**: Run this command while building your derived image. e.g. ```INSTALL1337="apt-get update -y && apt-get upgrade -y"```
- Your image is now built. It is cached in .tinybuild/image_name-$(image_md5)-$(commands_md5), where commands_md5 is the hash of the commands you ran to get there.
- **COPYxx**: Copies a file from the host to the container on every run. e.g. ```COPYabc="src:/whatever/src"```
- **EXEC**: The real building command to run after all the COPYs are done. e.g. ```EXEC="make"```.
- **POSTCOPYxx**: Copies a file from the container to the host on every run. e.g. ```POSTCOPY="/build/artifact:artifact"```
