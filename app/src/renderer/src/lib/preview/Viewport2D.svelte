<script lang="ts">
	import {
		Color,
		DataTexture,
		FloatType,
		Mesh,
		NearestFilter,
		OrthographicCamera,
		PlaneGeometry,
		RedFormat,
		Scene,
		type Points,
		ShaderMaterial,
		Vector2,
		WebGLRenderer
	} from 'three';
	import { onMount } from 'svelte';
	import { readColorToken } from '$lib/design-tokens';
	import {
		ambientShade,
		sunDirection,
		topDownFragmentShader,
		topDownVertexShader
	} from './hillshade';
	import {
		applyDrape,
		applyMaterialRepeat,
		applyMaterialTiles,
		applyMaterialWeights,
		buildPointMarkers,
		colorTexture,
		maskTexture,
		materialUniforms,
		materialWeightTexture
	} from './drapes';
	import { previewState } from './preview-state.svelte';
	import { surfaceTextures } from './surface-textures';

	let container = $state<HTMLDivElement | null>(null);

	const scene = new Scene();
	const camera = new OrthographicCamera(-1, 1, 1, -1, 0.1, 100);
	camera.position.set(0, 0, 10);

	const material = new ShaderMaterial({
		vertexShader: topDownVertexShader,
		fragmentShader: topDownFragmentShader,
		uniforms: {
			heights: { value: null },
			texelSize: { value: new Vector2(1, 1) },
			cellSizeMeters: { value: 1 },
			sunDirection: { value: sunDirection(315, 45) },
			lowestAltitude: { value: 0 },
			highestAltitude: { value: 1 },
			ambient: { value: ambientShade },
			overlay: { value: null },
			overlayStrength: { value: 0 },
			painted: { value: null },
			paintedStrength: { value: 0 },
			...materialUniforms()
		}
	});

	let overlayTexture: DataTexture | null = null;
	let paintedTexture: DataTexture | null = null;
	let weightTexture: DataTexture | null = null;
	let markers: Points | null = null;

	let renderer: WebGLRenderer | null = null;
	let quad: Mesh | null = null;
	let texture: DataTexture | null = null;
	let spanMeters = 1;
	/// Pan and zoom, held outside the reactive graph because effects both read
	/// and write them. `viewTransform` is a write-only mirror for the crosshair.
	const centre = new Vector2(0, 0);
	let pixelsPerMeter = 1;
	let viewTransform = $state({ centreX: 0, centreY: 0, pixelsPerMeter: 1 });
	let dragOrigin: { x: number; y: number } | null = null;

	function canvasSize(): { width: number; height: number } {
		if (!container) {
			return { width: 1, height: 1 };
		}
		return {
			width: Math.max(container.clientWidth, 1),
			height: Math.max(container.clientHeight, 1)
		};
	}

	function applyCamera(): void {
		const { width, height } = canvasSize();
		camera.left = -width / 2;
		camera.right = width / 2;
		camera.top = height / 2;
		camera.bottom = -height / 2;
		camera.zoom = pixelsPerMeter;
		camera.position.set(centre.x, centre.y, 10);
		camera.updateProjectionMatrix();
		viewTransform = { centreX: centre.x, centreY: centre.y, pixelsPerMeter };
	}

	function fitToSpan(): void {
		const { width, height } = canvasSize();
		centre.set(0, 0);
		pixelsPerMeter = (Math.min(width, height) * 0.92) / spanMeters;
		applyCamera();
	}

	function resize(): void {
		if (!renderer) {
			return;
		}
		const { width, height } = canvasSize();
		renderer.setSize(width, height, false);
		applyCamera();
	}

	function groundAt(event: PointerEvent): Vector2 | null {
		if (!renderer) {
			return null;
		}
		const bounds = renderer.domElement.getBoundingClientRect();
		return new Vector2(
			centre.x + (event.clientX - bounds.left - bounds.width / 2) / pixelsPerMeter,
			centre.y - (event.clientY - bounds.top - bounds.height / 2) / pixelsPerMeter
		);
	}

	function reportCursor(event: PointerEvent): void {
		const field = previewState.field;
		const ground = groundAt(event);
		if (!field || !ground) {
			return;
		}
		const easting = (ground.x + spanMeters / 2) / spanMeters;
		const northing = (ground.y + spanMeters / 2) / spanMeters;
		previewState.setCursorCell(
			(1 - northing) * field.height - 0.5,
			easting * field.width - 0.5
		);
	}

	function beginPan(event: PointerEvent): void {
		dragOrigin = { x: event.clientX, y: event.clientY };
		(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId);
	}

	function continuePan(event: PointerEvent): void {
		reportCursor(event);
		if (!dragOrigin) {
			return;
		}
		centre.set(
			centre.x - (event.clientX - dragOrigin.x) / pixelsPerMeter,
			centre.y + (event.clientY - dragOrigin.y) / pixelsPerMeter
		);
		dragOrigin = { x: event.clientX, y: event.clientY };
		applyCamera();
	}

	function endPan(event: PointerEvent): void {
		dragOrigin = null;
		(event.currentTarget as HTMLElement).releasePointerCapture(event.pointerId);
	}

	/// Zooms about the pointer, so the cell under it stays put.
	function zoom(event: WheelEvent): void {
		event.preventDefault();
		const anchor = groundAt(event as unknown as PointerEvent);
		if (!anchor) {
			return;
		}
		const factor = Math.exp(-event.deltaY * 0.0015);
		pixelsPerMeter = Math.max(pixelsPerMeter * factor, 0.001);
		centre.set(anchor.x + (centre.x - anchor.x) / factor, anchor.y + (centre.y - anchor.y) / factor);
		applyCamera();
	}

	// Renderer setup runs once; pan and zoom are state the crosshair reads, and
	// an $effect that touched them would tear the context down on every pan.
	onMount(() => {
		const host = container!;
		scene.background = new Color(readColorToken('--bg'));
		renderer = new WebGLRenderer({ antialias: false });
		renderer.setPixelRatio(window.devicePixelRatio);
		host.appendChild(renderer.domElement);

		renderer.setAnimationLoop(() => renderer?.render(scene, camera));

		const observer = new ResizeObserver(resize);
		observer.observe(host);
		resize();

		return () => {
			observer.disconnect();
			renderer?.setAnimationLoop(null);
			texture?.dispose();
			quad?.geometry.dispose();
			renderer?.dispose();
			renderer?.domElement.remove();
			renderer = null;
		};
	});

	$effect(() => {
		const field = previewState.field;
		if (!field) {
			return;
		}
		texture?.dispose();
		texture = new DataTexture(field.samples, field.width, field.height, RedFormat, FloatType);
		texture.minFilter = NearestFilter;
		texture.magFilter = NearestFilter;
		texture.needsUpdate = true;

		spanMeters = field.width * field.cellSizeMeters;
		material.uniforms.heights.value = texture;
		material.uniforms.texelSize.value = new Vector2(1 / field.width, 1 / field.height);
		material.uniforms.cellSizeMeters.value = field.cellSizeMeters;
		material.uniforms.lowestAltitude.value = field.minimum;
		material.uniforms.highestAltitude.value = field.maximum;

		if (quad) {
			scene.remove(quad);
			quad.geometry.dispose();
		}
		quad = new Mesh(new PlaneGeometry(spanMeters, spanMeters), material);
		scene.add(quad);
		fitToSpan();
	});


	$effect(() => {
		const overlay = previewState.overlay;
		let next: DataTexture | null = null;
		if (overlay) {
			next = maskTexture(overlay);
		}
		overlayTexture = applyDrape(material, overlayTexture, next, 'overlay');
	});

	$effect(() => {
		const painted = previewState.painted;
		let next: DataTexture | null = null;
		if (painted) {
			next = colorTexture(painted);
		}
		paintedTexture = applyDrape(material, paintedTexture, next, 'painted');
	});

	$effect(() => {
		const materials = previewState.materials;
		let next: DataTexture | null = null;
		if (materials) {
			next = materialWeightTexture(materials.weights);
		}
		weightTexture = applyMaterialWeights(material, weightTexture, next);
		if (!materials) {
			return;
		}
		surfaceTextures(materials.textureNames).then((tiles) => {
			if (previewState.materials === materials) {
				applyMaterialTiles(material, tiles);
			}
		});
	});

	$effect(() => {
		applyMaterialRepeat(material, previewState.worldSizeMeters, previewState.tileSizeMeters);
	});

	/// The same markers the 3D view shows, laid flat just above the map so they
	/// are not swallowed by the quad.
	$effect(() => {
		const points = previewState.points;
		const field = previewState.field;
		if (markers) {
			scene.remove(markers);
			markers.geometry.dispose();
			markers = null;
		}
		if (!points || !field || points.count === 0) {
			return;
		}
		markers = buildPointMarkers(points, field, spanMeters / 200, readColorToken('--ctx-green'));
		markers.position.z = 1;
		markers.rotation.x = Math.PI / 2;
		scene.add(markers);
	});

	$effect(() => {
		material.uniforms.sunDirection.value = sunDirection(
			previewState.sunAzimuthDegrees,
			previewState.sunAltitudeDegrees
		);
	});

	/// The crosshair mirrors whichever view the pointer is in.
	const crosshair = $derived.by(() => {
		const field = previewState.field;
		const cursor = previewState.cursor;
		const view = viewTransform;
		if (!field || !cursor || !container) {
			return null;
		}
		const groundX = ((cursor.column + 0.5) / field.width - 0.5) * spanMeters;
		const groundY = (0.5 - (cursor.row + 0.5) / field.height) * spanMeters;
		const { width, height } = canvasSize();
		return {
			left: width / 2 + (groundX - view.centreX) * view.pixelsPerMeter,
			top: height / 2 - (groundY - view.centreY) * view.pixelsPerMeter
		};
	});
</script>

<div
	class="relative h-full w-full"
	role="application"
	aria-label="Top-down terrain preview"
	bind:this={container}
	onpointerdown={beginPan}
	onpointermove={continuePan}
	onpointerup={endPan}
	onpointerleave={() => previewState.clearCursor()}
	onwheel={zoom}
>
	{#if crosshair}
		<div
			class="pointer-events-none absolute h-3 w-3 -translate-x-1/2 -translate-y-1/2 rounded-full border border-line-strong"
			style="left: {crosshair.left}px; top: {crosshair.top}px"
		></div>
	{/if}
</div>
