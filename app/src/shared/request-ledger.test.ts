import { describe, expect, test } from 'bun:test';
import { RequestLedger } from './request-ledger';

describe('RequestLedger', () => {
	test('hands out increasing request ids', () => {
		const ledger = new RequestLedger();

		expect(ledger.open().requestId).toBe(1);
		expect(ledger.open().requestId).toBe(2);
	});

	test('resolves the request the response names, out of order', async () => {
		const ledger = new RequestLedger();
		const first = ledger.open();
		const second = ledger.open();

		ledger.settle({ requestId: second.requestId, status: 'ok', result: 'second' });
		ledger.settle({ requestId: first.requestId, status: 'ok', result: 'first' });

		expect(await first.result).toBe('first');
		expect(await second.result).toBe('second');
		expect(ledger.pendingCount).toBe(0);
	});

	test('rejects on an error response', async () => {
		const ledger = new RequestLedger();
		const request = ledger.open();

		ledger.settle({ requestId: request.requestId, status: 'error', message: 'no such node' });

		await expect(request.result).rejects.toThrow('no such node');
	});

	test('ignores a response for an unknown request id', () => {
		const ledger = new RequestLedger();

		ledger.settle({ requestId: 99, status: 'ok', result: null });

		expect(ledger.pendingCount).toBe(0);
	});

	test('fails everything in flight when the engine dies', async () => {
		const ledger = new RequestLedger();
		const request = ledger.open();

		ledger.failAll(new Error('engine crashed'));

		await expect(request.result).rejects.toThrow('engine crashed');
		expect(ledger.pendingCount).toBe(0);
	});
});
