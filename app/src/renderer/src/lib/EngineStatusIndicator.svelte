<script lang="ts">
	import { StatusBadge } from '@neoworks-dev/ui';
	import type { StatusTone } from '@neoworks-dev/ui';
	import { engineClient } from './engine-client.svelte';

	const tones: Record<string, StatusTone> = {
		starting: 'amber',
		ready: 'green',
		crashed: 'amber',
		unavailable: 'red'
	};

	const label = $derived.by(() => {
		if (engineClient.status === 'ready' && engineClient.version) {
			return `Engine ${engineClient.version}`;
		}
		if (engineClient.status === 'ready') {
			return 'Engine ready';
		}
		if (engineClient.status === 'crashed') {
			return 'Engine crashed — restarting';
		}
		if (engineClient.status === 'unavailable') {
			return 'Engine will not start';
		}
		return 'Engine starting';
	});
</script>

<StatusBadge tone={tones[engineClient.status]}>{label}</StatusBadge>
