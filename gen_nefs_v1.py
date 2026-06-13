import os

content = "// NEFS v1.0 placeholder\n"
path = r"D:\Project\EmotionOS\kernel\include\emotionos\nefs\nefs.h"
with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("Placeholder written")
