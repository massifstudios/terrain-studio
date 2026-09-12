Source: https://docs.gaea.app/using/using-gaea/build-and-export/exporting-nodes.html

# Exporting Nodes

Gaea is primarily an asset generator designed for creating and exporting terrains to be used in other applications. It supports all major heightfield and mesh formats, ensuring compatibility with various platforms and rendering tools.

![Output/Export Nodes in the Gaea Toolbox](https://docs.gaea.app/.data/assets/image-(42).png)

## **Output Nodes**

![Mesher node properties](https://docs.gaea.app/.data/assets/image-(41).png) ![Export node for Bitmap export](https://docs.gaea.app/.data/assets/image-(3).png)

- **Node Types**: Utilize specific output nodes, such as Mesher for mesh generation, Point Cloud for simple XY point cloud export, or platform-specific nodes like [Unreal](https://docs.gaea.app/reference/nodes/output/unreal.html) and [Unity](https://docs.gaea.app/reference/nodes/output/unity.html), to export your assets.
- **Customization**: Depending on the output node type, you can adjust settings such as map sizes for Unreal, file naming conventions, or mesh topology and vertex count for Mesher.
- **Naming Conventions**: The name of the exported file matches the name of the Output node, streamlining asset management and integration into other projects.

### **Mark for Export**

![Data View's BUILD Tab showing nodes marked for export.](https://docs.gaea.app/.data/assets/image-(39).png)

- **Shortcut Key**: Right-click a node and select `Mark for Export` or press `F3` on any node to mark it for export. This flexibility allows you to export multiple node outputs simultaneously.
- **Build Tab and Settings**: The Build tab and Build Settings window display all nodes marked for export. Here, you can change the output format, select which ports to export, and rename the export files.
- **File Naming**: Exported files are named in the format `<Name or NodeName>_<PortName>.<extension>`, ensuring clarity and order in file management.

You can disable `_Out` suffix for the Primary Port in [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html).

### Export Settings and Options

![](https://docs.gaea.app/.data/assets/image-(4).png)

**File name.** The node name is used by default for the export filename, however, you change the name in the Export Items list in the Build Tab in [Data Editor](https://docs.gaea.app/ui/interface/data-editor/index.html) or in the [Build Options](https://docs.gaea.app/ui/interface/build-options/index.html).

**File Formats.** Gaea supports a wide range of file formats, allowing you to choose the best option based on the requirements of the receiving application or your workflow needs.

**Port Selection.** From the export menu, you can select specific ports (such as Enabled, Out, Flow, Wear, or Deposits) to export, providing control over the data being exported and optimizing resource usage.

**Additional Options**

- **Primary Only.** Export only the primary or most important outputs of a node, reducing the number of files and focusing on key data.
- **Mark All/None.** Quickly select or deselect all available export options for efficiency and convenience.

For automation scenarios where you wish to explicitly name the output file, see [Managing Input and Output](https://docs.gaea.app/developers/automation/building-bridges/managing-input-and-output.html) in [Automation](https://docs.gaea.app/developers/automation/index.html)
