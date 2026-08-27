<script lang="ts">
	import {
		Color,
		DataTexture,
		Mesh,
		MeshBasicMaterial,
		SphereGeometry,
		PerspectiveCamera,
		Raycaster,
		Scene,
		ShaderMaterial,
		Vector2,
		Vector3,
		WebGLRenderer,
		type Points,
		type PlaneGeometry
	} from 'three';
	import { onMount } from 'svelte';
	import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
	import { readColorToken } from '$lib/design-tokens';
	import {
		ambientShade,
		sunDirection,
		surfaceFragmentShader,
		surfaceVertexShader
	} from './hillshade';
	import { applyDrape, buildPointMarkers, colorTexture, maskTexture } from './drapes';
	import { previewState } from './preview-state.svelte';
	import { pickHeightfieldCell } from './heightfield-pick';
	import { buildTerrainGeometry, spanMetersOf } from './terrain-mesh';

	let container = $state<HTMLDivElement | null>(null);

	const scene = new Scene();

	const camera = new PerspectiveCamera(45, 1, 1, 500000);
	const raycaster = new Raycaster();
	const pointer = new Vector2();

	const material = new ShaderMaterial({
		vertexShader: surfaceVertexShader,
		fragmentShader: surfaceFragmentShader,
		uniforms: {
			sunDirection: { value: sunDirection(315, 45) },
			lowestAltitude: { value: 0 },
			highestAltitude: { value: 1 },
			ambient: { value: ambientShade },
			overlay: { value: null },
			overlayStrength: { value: 0 },
			painted: { value: null },
			paintedStrength: { value: 0 }
		}
	});

	let overlayTexture: DataTexture | null = null;
	let paintedTexture: DataTexture | null = null;
	let markers: Points | null = null;

	let renderer: WebGLRenderer | null = null;
	let controls: OrbitControls | null = null;
	let mesh: Mesh | null = null;
	let cursorMarker: Mesh | null = null;

	/// Mirrors the 2D view's crosshair: a marker standing on the same cell.
	function placeCursorMarker(): void {
		const field = previewState.field;
		const cursor = previewState.cursor;
		if (!cursorMarker) {
			return;
		}
		if (!field || !cursor) {
			cursorMarker.visible = false;
			return;
		}
		const span = spanMetersOf(field);
		cursorMarker.visible = true;
		cursorMarker.position.set(
			cursor.column * field.cellSizeMeters - span / 2,
			cursor.altitudeMeters,
			cursor.row * field.cellSizeMeters - span / 2
		);
	}

	function frameTerrain(span: number, lowest: number, highest: number): void {
		const centre = new Vector3(0, (lowest + highest) / 2, 0);
		camera.position.set(span * 0.55, highest + span * 0.45, span * 0.75);
		camera.near = span / 1000;
		camera.far = span * 10;
		camera.updateProjectionMatrix();
		if (controls) {
			controls.target.copy(centre);
			controls.update();
		}
	}

	function replaceMesh(geometry: PlaneGeometry): void {
		if (mesh) {
			scene.remove(mesh);
			mesh.geometry.dispose();
		}
		mesh = new Mesh(geometry, material);
		scene.add(mesh);
	}

	function resize(): void {
		if (!renderer || !container) {
			return;
		}
		const width = Math.max(container.clientWidth, 1);
		const height = Math.max(container.clientHeight, 1);
		renderer.setSize(width, height, false);
		camera.aspect = width / height;
		camera.updateProjectionMatrix();
	}

	function reportCursor(event: PointerEvent): void {
		const field = previewState.field;
		if (!field || !renderer) {
			return;
		}
		const bounds = renderer.domElement.getBoundingClientRect();
		pointer.set(
			((event.clientX - bounds.left) / bounds.width) * 2 - 1,
			-((event.clientY - bounds.top) / bounds.height) * 2 + 1
		);
		raycaster.setFromCamera(pointer, camera);
		const cell = pickHeightfieldCell(field, raycaster.ray);
		if (!cell) {
			previewState.clearCursor();
			return;
		}
		previewState.setCursorCell(cell.row, cell.column);
	}

	// Renderer setup runs once. An $effect here would re-create the context
	// every time anything it happened to read changed.
	onMount(() => {
		const host = container!;
		scene.background = new Color(readColorToken('--bg'));
		renderer = new WebGLRenderer({ antialias: true });
		renderer.setPixelRatio(window.devicePixelRatio);
		host.appendChild(renderer.domElement);

		controls = new OrbitControls(camera, renderer.domElement);
		controls.enableDamping = true;

		renderer.setAnimationLoop(() => {
			controls?.update();
			renderer?.render(scene, camera);
		});

		cursorMarker = new Mesh(
			new SphereGeometry(1, 12, 8),
			new MeshBasicMaterial({ color: readColorToken('--text') })
		);
		cursorMarker.visible = false;
		scene.add(cursorMarker);

		const observer = new ResizeObserver(resize);
		observer.observe(host);
		resize();

		return () => {
			observer.disconnect();
			renderer?.setAnimationLoop(null);
			controls?.dispose();
			mesh?.geometry.dispose();
			cursorMarker?.geometry.dispose();
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
		material.uniforms.lowestAltitude.value = field.minimum;
		material.uniforms.highestAltitude.value = field.maximum;
		replaceMesh(buildTerrainGeometry(field));
		const span = spanMetersOf(field);
		cursorMarker?.scale.setScalar(span / 250);
		frameTerrain(span, field.minimum, field.maximum);
	});

	$effect(() => {
		placeCursorMarker();
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

	/// One marker per scattered instance, standing on the surface below it.
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
		markers = buildPointMarkers(
			points,
			field,
			spanMetersOf(field) / 220,
			readColorToken('--ctx-green')
		);
		scene.add(markers);
	});

	$effect(() => {
		material.uniforms.sunDirection.value = sunDirection(
			previewState.sunAzimuthDegrees,
			previewState.sunAltitudeDegrees
		);
	});
</script>

<div
	class="h-full w-full"
	role="application"
	aria-label="Three-dimensional terrain preview"
	bind:this={container}
	onpointermove={reportCursor}
	onpointerleave={() => previewState.clearCursor()}
></div>
