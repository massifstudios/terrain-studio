<!--
	The Build button. How far a build has come, how long it took and why it
	failed are the status bar's business, not this button's.
-->
<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import Play from 'phosphor-svelte/lib/Play';
	import X from 'phosphor-svelte/lib/X';
	import { buildRunner } from './build-runner.svelte';
	import { engineClient } from './engine-client.svelte';
</script>

{#if buildRunner.isBuilding}
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
