Source: https://docs.gaea.app/reference/nodes/simulate/anastomosis.html

# Anastomosis

Simulate › Erosion

Shortcode `an`

Anastomosis creates interconnected water flow based extractions, ranging from small pits to large river channels. It is useful for both terrains and textures.

---

Anastomosis creates interconnected water flow based downcutting, ranging from small pits to large river channels. It can be used for roughening up a surface without losing the features of that terrain, and for deepening existing flow structures.

Before and after Anastomosis.

![](https://docs.gaea.app/.data/ref/Anastomosis/Anastomosis--1.webp)

![](https://docs.gaea.app/.data/ref/Anastomosis/Anastomosis--2.webp)

Using high Impact and Influence values in Destructive mode.

![](https://docs.gaea.app/.data/ref/Anastomosis/Anastomosis--HighImpact-Influence.webp)

Using a high Impact value in Rivers mode.

![](https://docs.gaea.app/.data/ref/Anastomosis/Anastomosis--RiversImpact.webp)

## When to Use It

Use Anastomosis when you need interconnected water flow based downcutting, ranging from small pits to large river channels.

## Properties

**Impact** (`Single`)  
The intensity of the process. Higher values create stronger physical impacts within the erosion process.

**Width** (`Single`)  
Controls the horizontal spread of the anastomosis effect. Larger values produce wider channels and features.

**Depth** (`Single`)  
Determines the vertical extent of the erosion. Higher values result in deeper cuts and more pronounced terrain features.

**Downcutting** (`Enum`)  
Specifies the style and aggressiveness of vertical erosion. Select from various modes to control how the terrain is carved downward.

- **Destructive** — Applies aggressive downcutting, rapidly eroding and deepening channels.
- **Pits** — Focuses erosion into localized depressions, creating pit-like features.
- **Flows** — Simulates water flow erosion, carving smooth, continuous channels.
- **Rivers** — Emphasizes river-like downcutting, forming distinct riverbeds.
- **Riversx 2** — Doubles the intensity of river downcutting for deeper, more defined riverbeds.
- **Riversx 4** — Quadruples the river downcutting effect, producing extremely deep and prominent river channels.
- **Flooding** — Simulates broad, shallow erosion typical of flooding events.
- **Floodingx 2** — Doubles the flooding effect, increasing the area and depth of flood erosion.
- **Flooding X4** — Quadruples the flooding effect, creating extensive and deep flood-eroded regions.

**Quality** (`Enum`)  
Sets the processing resolution for the node. Higher quality yields more detailed results at the cost of performance.

- **Quarter** — Processes at 1/4 resolution for fastest performance and lowest detail.
- **Third** — Processes at 1/3 resolution, balancing speed and detail.
- **Half** — Processes at 1/2 resolution for moderate detail and performance.
- **Full** — Processes at full resolution for maximum detail and slower performance.

**Smooth Edges** (`Boolean`)  
Softens the edges of eroded features, reducing sharp transitions and creating a more natural appearance.
