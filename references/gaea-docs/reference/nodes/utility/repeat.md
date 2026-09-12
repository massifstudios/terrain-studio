Source: https://docs.gaea.app/reference/nodes/utility/repeat.html

# Repeat

Utility › Structure

Shortcode `rp`

Repeat takes a shape and repeats it in a tiled pattern. It is often combined with the Seamless node.

This simple filter repeats or tiles a given terrain. You can choose the number of times it is repeated.

In most cases, you may want to use Seamless before Repeat to let the edges match seamlessly.

## When to Use It

Use Repeat when you want a single shape or motif duplicated across the terrain in a controllable tiled arrangement.

## Properties

**Tiles** (`Int32`)  
The number of repetitions on either axis. Controls how many times the input is tiled horizontally and vertically across the terrain.

**Compensate Height** (`Boolean`)  
Adjusts the height of each tile to prevent elevation buildup at tile seams, ensuring a smoother and more seamless repeat pattern.

**Height** (`Single`)  
Sets the maximum elevation for the repeated tiles, allowing you to control the overall height of the tiled output.
