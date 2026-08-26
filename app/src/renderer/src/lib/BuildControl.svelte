<!--
	The Build button and what a build has to say for itself.

	`Button`, `StatusBadge` and `Tooltip` come from the design system. The
	progress bar does not: the system has `LoadingSpinner`, which says only
	"working", and a build that takes minutes needs to say how far along it is.
	It is two divs and a width, drawn in the system's tokens.
-->
<script lang="ts">
	import { Button, StatusBadge, Tooltip } from '@neoworks-dev/ui';
	import type { StatusTone } from '@neoworks-dev/ui';
	import Play from 'phosphor-svelte/lib/Play';
	import X from 'phosphor-svelte/lib/X';
	import { buildRunner } from './build-runner.svelte';
	import { engineClient } from './engine-client.svelte';

	const fraction = $derived.by(() => {
		if (buildRunner.total === 0) {
			return 0;
		}
		return buildRunner.completed / buildRunner.total;
	});

	const tones: Record<string, StatusTone> = {
		running: 'amber',
		finished: 'green',
		failed: 'red',
		cancelled: 'neutral'
	};

	const label = $derived.by(() => {
		if (buildRunner.state === 'running') {
			return `${buildRunner.completed + 1} of ${buildRunner.total}`;
		}
		if (buildRunner.state === 'finished') {
			return `Built ${buildRunner.total} outputs`;
		}
		if (buildRunner.state === 'cancelled') {
			return 'Build cancelled';
		}
		return 'Build failed';
	});

	/// While it runs, what the engine is chewing on; afterwards, what went
	/// wrong, in full.
	const detail = $derived.by(() => {
		if (buildRunner.state === 'running' && engineClient.progress) {
			return `${engineClient.progress.typeName} — ${Math.round(engineClient.progress.fraction * 100)}%`;
		}
		if (buildRunner.errors.length > 0) {
			return buildRunner.errors.join('\n');
		}
		return label;
	});
</script>

<div class="flex items-center gap-3">
	{#if buildRunner.state !== 'idle'}
		<Tooltip text={detail}>
			<StatusBadge tone={tones[buildRunner.state]}>{label}</StatusBadge>
		</Tooltip>
	{/if}

	{#if buildRunner.isBuilding}
		<div class="h-1 w-20 overflow-hidden rounded-full bg-input">
			<div class="h-full bg-action" style="width: {fraction * 100}%"></div>
		</div>
		<Button size="sm" variant="surface" icon={X} onclick={() => buildRunner.cancel()}>Cancel</Button>
	{:else}
		<Button
			size="sm"
			variant="primary"
			icon={Play}
			disabled={!engineClient.isReady}
			onclick={() => buildRunner.start()}
		>
			Build
		</Button>
	{/if}
</div>
