<script lang="ts">
	import { Button, Select } from '@neoworks-dev/ui';
	import ArrowsClockwise from 'phosphor-svelte/lib/ArrowsClockwise';
	import FloppyDisk from 'phosphor-svelte/lib/FloppyDisk';
	import FolderOpen from 'phosphor-svelte/lib/FolderOpen';
	import Play from 'phosphor-svelte/lib/Play';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import { parseGraph, serializeGraph } from '$shared/massifgraph';
	import EngineStatusIndicator from './EngineStatusIndicator.svelte';
	import PluginIndicator from './PluginIndicator.svelte';

	const previewResolutions = ['256', '512', '1024', '2048'];
	const buildResolutions = ['2048', '4096', '8192', '10240'];

	function toOptions(values: string[]) {
		return values.map((value) => ({ value, label: value }));
	}

	const projectName = $derived.by(() => {
		if (!graphStore.filePath) {
			return 'Untitled';
		}
		const cut = Math.max(graphStore.filePath.lastIndexOf('/'), graphStore.filePath.lastIndexOf('\\'));
		return graphStore.filePath.slice(cut + 1);
	});

	async function openProject(): Promise<void> {
		const path = await window.fileBridge.chooseOpenPath('graph');
		if (path === null) {
			return;
		}
		graphStore.load(parseGraph(await window.fileBridge.readText(path)), path);
	}

	async function saveProject(): Promise<void> {
		let path = graphStore.filePath;
		if (path === null) {
			path = await window.fileBridge.chooseSavePath('graph', 'terrain.massifgraph');
		}
		if (path === null) {
			return;
		}
		await window.fileBridge.writeText(path, serializeGraph(graphStore.project()));
		graphStore.markSaved(path);
	}

	function rerollSeed(): void {
		graphStore.settings.seed = Math.floor(Math.random() * 0xffffffff);
	}
</script>

<header class="flex h-12 shrink-0 items-center gap-3 border-b border-line bg-elevated px-4">
	<span class="text-sm font-medium tracking-tight">Terrain Studio</span>
	<span class="text-2xs text-faint">
		{projectName}{#if graphStore.hasUnsavedChanges}&nbsp;•{/if}
	</span>

	<div class="flex items-center gap-2">
		<Button size="sm" variant="surface" icon={FolderOpen} onclick={openProject}>Open</Button>
		<Button size="sm" variant="surface" icon={FloppyDisk} onclick={saveProject}>Save</Button>
	</div>

	<div class="ml-auto flex items-center gap-4">
		<label class="flex items-center gap-2 text-2xs text-muted">
			Seed
			<input
				class="w-24 rounded-sm border border-line bg-input px-2 py-1 text-right font-mono text-2xs text-default outline-none focus:border-line-strong"
				type="number"
				value={graphStore.settings.seed}
				onchange={(event) => (graphStore.settings.seed = Number(event.currentTarget.value))}
			/>
			<Button size="sm" variant="ghost" icon={ArrowsClockwise} onclick={rerollSeed}>Roll</Button>
		</label>

		<label class="flex items-center gap-2 text-2xs text-muted">
			World
			<input
				class="w-24 rounded-sm border border-line bg-input px-2 py-1 text-right font-mono text-2xs text-default outline-none focus:border-line-strong"
				type="number"
				value={graphStore.settings.worldSizeMeters}
				onchange={(event) =>
					(graphStore.settings.worldSizeMeters = Number(event.currentTarget.value))}
			/>
			m
		</label>

		<label class="flex items-center gap-2 text-2xs text-muted">
			Preview
			<Select
				value={String(graphStore.settings.resolution)}
				options={toOptions(previewResolutions)}
				onChange={(value) => (graphStore.settings.resolution = Number(value))}
			/>
		</label>

		<label class="flex items-center gap-2 text-2xs text-muted">
			Build
			<Select
				value={String(graphStore.buildResolution)}
				options={toOptions(buildResolutions)}
				onChange={(value) => (graphStore.buildResolution = Number(value))}
			/>
		</label>

		<Button size="sm" variant="primary" icon={Play} disabled>Build</Button>
		<PluginIndicator />
		<EngineStatusIndicator />
	</div>
</header>
