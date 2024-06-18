# fill db history.license
import os
import glob

def run():
    folder_path = "/mnt/drive/PhysWiki-backup/"
    output = "/mnt/drive/PhysWikiScan/add_history_license.sql"
    tex_files = glob.glob(os.path.join(folder_path, "*.tex"))  
    sql = ""
    # Loop through each .tex file
    for tex_file in tex_files:
        filename = os.path.splitext(os.path.basename(tex_file))[0]
        time, author, entry = filename.split("_")
        lic = ""
        with open(tex_file, "r", encoding="utf-8") as file:
            # Read the first 5 lines of the file
            lines = file.readlines()[:5]
            # Find and print the line starting with "% license"
            for line in lines:
                if line.startswith("% license"):
                    lic = line.strip("% license").strip()
                    sql += f"update history set license = '{lic}' where time = {time} and author = {author} and entry = '{entry}';\n"
                    break  # Exit the loop after finding the line
    with open(output, 'w') as file:
        file.write(sql)
        
    print("done")
    
run()
