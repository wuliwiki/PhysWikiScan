import sys
import zlib
import base64
import diff_match_patch as dmp_module


file1 = '202107021611_0_AU.tex'
file2 = '202107021616_0_AU.tex'
file12 = 'diff.txt'

with open(file1, 'r') as file:
    str1 = file.read()
with open(file2, 'r') as file:
    str2 = file.read()

dmp = dmp_module.diff_match_patch()
diff = dmp.diff_main(str1, str2)
dmp.diff_cleanupSemantic(diff)
patches = dmp.patch_make(diff)

diff_text = dmp.patch_toText(patches)
with open('diff_text.txt', 'w') as file:
    file.write(diff_text)

diff_text_zip = zlib.compress(diff_text.encode('ascii'))
with open('diff_text_zip.txt', 'wb') as file:
    file.write(diff_text_zip)

# decompress
diff_text2 = zlib.decompress(diff_text_zip).decode('ascii')
with open('diff_text2.txt', 'w') as file:
    file.write(diff_text2)
