<?php 

class Stats extends BaseUnit
{
    public function test(){
        $keys = array
        (
            'server_version',
            'server_build_datetime',
            'server_allocator',
            'server_arch',
            'server_started',
            'server_time',
            'first_item_seen',
            'last_item_seen',
            'total_items',
            'total_compressed_items',
            'total_clients',
            'total_cron_done',
            'total_connections',
            'total_requests',
            'memory_available',
            'memory_usable',
            'memory_used',
            'memory_peak',
            'memory_fragmentation',
            'item_size_avg',
            'compr_rate_avg',
            'reqs_per_client_avg'
        );

        $stats = $this->client->stats();
        foreach( $keys as $k ){
            $this->assertIsSet( $stats, $k );
        }

        $now = time();

        $this->assertInArray( $stats['server_allocator'], array( 'malloc', 'jemalloc' ) );
        $this->assertInArray( $stats['server_arch'], array( 32, 64 ) );
        $this->assertBetween( $stats['server_started'], $now - 10, $now );
        $this->assertBetween( $stats['server_time'], $now - 1, $now );
        $this->assertBetween( $stats['first_item_seen'], 0, $now );
        $this->assertBetween( $stats['last_item_seen'], 0, $now );
        $this->assertEqual( $stats['total_clients'], 1 );
        $this->assertTrue( $stats['memory_fragmentation'] < 1 );
    }
}

?>
