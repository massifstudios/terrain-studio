import { builtinModules } from 'node:module';
import { resolve } from 'node:path';
import { defineConfig } from 'electron-vite';
import { svelte, vitePreprocess } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';

const nodeExternals = [
	'electron',
	/^electron\/.+/,
	...builtinModules.flatMap((name) => [name, `node:${name}`])
];

export default defineConfig({
	main: {
		build: {
			rollupOptions: {
				external: nodeExternals,
				input: {
					index: resolve(__dirname, 'src/main/index.ts'),
					'engine-host': resolve(__dirname, 'src/engine/host.ts')
				}
			}
		}
	},
	preload: {
		build: {
			rollupOptions: {
				external: nodeExternals,
				input: {
					index: resolve(__dirname, 'src/preload/index.ts')
				}
			}
		}
	},
	renderer: {
		root: resolve(__dirname, 'src/renderer'),
		build: {
			rollupOptions: {
				input: {
					index: resolve(__dirname, 'src/renderer/index.html')
				}
			}
		},
		resolve: {
			alias: {
				$lib: resolve(__dirname, 'src/renderer/src/lib'),
				$shared: resolve(__dirname, 'src/shared')
			}
		},
		plugins: [tailwindcss(), svelte({ preprocess: vitePreprocess() })]
	}
});
