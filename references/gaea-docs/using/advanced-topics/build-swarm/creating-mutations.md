Source: https://docs.gaea.app/using/advanced-topics/build-swarm/creating-mutations.html

# Creating Mutations

Mutations are a great way to create multiple variations of the same terrain with minimal effort.

In its most basic form, mutations update the seeds used by all nodes in the terrain unless a node is baked. When you use the `--Seed <int>` command line argument, it increments each seed in every node by that amount.

This provides a clean and predictable way to generate multiple mutations.

To prevent specific nodes from being affected, you can try [Baking Nodes](https://docs.gaea.app/using/using-gaea/baking-nodes/index.html).

## Using the Batch Build Manager

The fastest way to set up mutations, is to go to Build menu > Batch Build Manager and use the `Add Mutation` button to queue up as many mutations as you want.

![](https://docs.gaea.app/.data/assets/Batch_Build_Manager_08-19-54-PM.png)

If you find yourself repeating these builds, save the batch build as a Windows Batch (.bat) file.

Use the `[Mutation]` token in the [#build](https://docs.gaea.app/using-gaea/build-and-export/build-options/#build) Destination to add the current mutation as part of a folder name or subfolder.

For example:

`<Builds>\[Filename]\[+++]\[Mutation]`

or

`<Builds>\[Filename]\[Region]_[Mutation]`
