Source: https://docs.gaea.app/using/advanced-topics/technical-information/paths-and-storage.html

# Paths and Storage

## Data

Gaea stores all application data, excluding Projects, in a centralized directory known as the Data folder.

| Install Type | Flag | Typical Path | Data Folder Path |
| --- | --- | --- | --- |
| Non-Admin | `/CURRENTUSER` | `C:\Users\<user>\AppData\Local\Programs\Gaea 2.0` | `C:\Users\<user>\AppData\Local\Programs\Gaea 2.0\Data` |
| Admin | `/ALLUSERS` | `C:\Program Files\QuadSpinner\Gaea 2.0` | `C:\Users\<user>\AppData\Roaming\QuadSpinner\Gaea\2.0\` |

### **Standard Installation**

If Gaea is installed in the Program Files directory, the Data folder is located at:

`<UserFolder>\AppData\Roaming\QuadSpinner\Gaea\2.0`

### **Portable Installation**

The default location for a Portable Installation is:

`C:\Users\<username>\AppData\Local\Programs\Gaea 2.0`

If Gaea is installed in a portable location, the Data folder is located adjacent to the Gaea.exe file.

For example, if your install location is: `D:\Programs\CG\Gaea\` then the Data folder will be stored at `D:\Programs\CG\Gaea\Data\`

Whenever a new installation of Gaea is created, upon first run, Gaea will check if the install folder is writeable (ie, does it require Admin Privileges). If it is writeable it will create the `Data` folder there, otherwise it will create it in `AppData`.

## Your Gaea Files

These paths can be changed in the [Options](https://docs.gaea.app/ui/interface/options/index.html) > [Paths](https://docs.gaea.app/ui/interface/options/paths.html).

### Documents

This is where all Presets, Screenshots, and other Gaea documents are stored.

Default location: `C:\Users\<you>\Documents\Gaea\`

### Projects

The is where all Gaea project files are stored by default.

Default location: `C:\Users\<you>\Documents\Gaea\Projects\`

### Builds

This is where output files from builds are stored. It is denoted by the token `<Builds>` in [#build](https://docs.gaea.app/using-gaea/build-and-export/build-options/#build) Build Destination.

Default location: `C:\Users\<you>\Documents\Gaea\Builds\`
