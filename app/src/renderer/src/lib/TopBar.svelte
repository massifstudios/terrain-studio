<script lang="ts">
	import { Button, Select } from '@neoworks-dev/ui';
	import FolderOpen from 'phosphor-svelte/lib/FolderOpen';
	import FloppyDisk from 'phosphor-svelte/lib/FloppyDisk';
	import Play from 'phosphor-svelte/lib/Play';
	import EngineStatusIndicator from './EngineStatusIndicator.svelte';

	const resolutionOptions = [
		{ value: '512', label: '512' },
		{ value: '1024', label: '1024' },
		{ value: '2048', label: '2048' }
	];

	const buildResolutionOptions = [
		{ value: '2048', label: '2048' },
		{ value: '4096', label: '4096' },
		{ value: '8192', label: '8192' },
		{ value: '10240', label: '10240' }
	];

	let previewResolution = $state('1024');
	let buildResolution = $state('8192');
</script>

<header
	class="flex h-12 shrink-0 items-center gap-3 border-b border-line bg-elevated px-4"
>
	<span class="text-sm font-medium tracking-tight">Terrain Studio</span>

	<div class="flex items-center gap-2">
		<Button size="sm" variant="surface" icon={FolderOpen}>Open</Button>
		<Button size="sm" variant="surface" icon={FloppyDisk}>Save</Button>
	</div>

	<div class="ml-auto flex items-center gap-4">
		<label class="flex items-center gap-2 text-xs text-muted">
			Preview
			<Select
				value={previewResolution}
				onChange={(value) => (previewResolution = value as string)}
				options={resolutionOptions}
			/>
		</label>
		<label class="flex items-center gap-2 text-xs text-muted">
			Build
			<Select
				value={buildResolution}
				onChange={(value) => (buildResolution = value as string)}
				options={buildResolutionOptions}
			/>
		</label>
		<Button size="sm" variant="primary" icon={Play} disabled>Build</Button>
		<EngineStatusIndicator />
	</div>
</header>
