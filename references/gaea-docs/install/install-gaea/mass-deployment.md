Source: https://docs.gaea.app/install/install-gaea/mass-deployment.html

# Mass Deployment

Gaea can be deployed to a large number of workstations in multiple ways.

When installing to a network drive, it must be installed on a network share hosted as a drive and not a UNC path. Gaea may not work properly on UNC paths.

## Unattended Installation

There are two ways to perform multiple installations / mass deployment - through a command-line sequence using or using a zip file for xcopy style deployment.

In either case, you should prepare an activation file or a copy of the offline license (see [License Types](https://docs.gaea.app/install/license-management/license-types.html) for more information)

### Create an Activation File

1. Create a text file named `activate.lic` (Notepad is fine).
2. Put **only** the license key in the file (no extra spaces, no additional lines).
3. Copy `activate.lic` to the **Gaea Data Folder** (see [Paths and Storage](https://docs.gaea.app/using/advanced-topics/technical-information/paths-and-storage.html)).
   - For mass deployment, place it in each user’s `AppData > Gaea Data` folder, or in the install folder as appropriate for your deployment setup.

On launch, Gaea will check if a license is already validated. If not, it will attempt to read and activate using `activate.lic`.

## Command Line Installation

Run the install (typically `Gaea-2.x.x.x.exe`) either via UI or with command line

Command Line example:

```
Gaea-2.2.7.0.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-

  ## additional flags
  /ALLUSERS - install for all users (administrative install).
  /CURRENTUSER - install for current user only (non-administrative install).
  /LOG="filename" - to save a log to a specific file.
  /DIR="X:\PATH" - override the installation folder.
```

Once the installation is finished, copy the license file to the Data Folder.

| Install Type | Flag | Typical Path | Data Folder Path |
| --- | --- | --- | --- |
| Non-Admin | `/CURRENTUSER` | `C:\Users\<user>\AppData\Local\Programs\Gaea 2.0` | `C:\Users\<user>\AppData\Local\Programs\Gaea 2.0\Data` |
| Admin | `/ALLUSERS` | `C:\Program Files\QuadSpinner\Gaea 2.0` | `C:\Users\<user>\AppData\Roaming\QuadSpinner\Gaea\2.0\` |

If Gaea has not run yet, it may not have created the folder.
