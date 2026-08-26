<!--
	The open project file changed on disk while there were unsaved edits, so
	there are two answers and neither is safe to guess.

	A bar rather than a design-system component: the system has no banner or
	dialog, and `Card` is a surface for content rather than a strip across the
	window, `StatusBadge` says a word and takes no action, and `ListRow` is a
	row in a list. Type, colour and spacing come from the system's tokens.
-->
<script lang="ts">
	import { Button } from '@neoworks-dev/ui';
	import { projectWatcher } from './project-watch.svelte';

	const fileName = $derived.by(() => {
		const path = projectWatcher.conflict?.path;
		if (!path) {
			return '';
		}
		const cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
		return path.slice(cut + 1);
	});
</script>

{#if projectWatcher.conflict}
	<div class="flex h-10 shrink-0 items-center gap-3 border-b border-line bg-elevated px-4">
		<span class="text-2xs text-default">
			{fileName} changed on disk, and this graph has unsaved edits.
		</span>
		<div class="ml-auto flex items-center gap-2">
			<Button size="sm" variant="surface" onclick={() => projectWatcher.reload()}>
				Reload from disk
			</Button>
			<Button size="sm" variant="surface" onclick={() => projectWatcher.overwrite()}>
				Overwrite with mine
			</Button>
		</div>
	</div>
{/if}
