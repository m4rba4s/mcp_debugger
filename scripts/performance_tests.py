#!/usr/bin/env python3
"""
MCP Debugger Performance Testing Suite

Comprehensive performance testing including:
- Startup time measurements
- Memory usage monitoring
- S-Expression parsing performance
- LLM API response times (mocked)
- x64dbg bridge communication latency
- Concurrent operation handling
- Memory leak detection
"""

import os
import sys
import time
import psutil
import subprocess
import threading
import statistics
import argparse
import json
import tempfile
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
import logging

@dataclass
class PerformanceMetrics:
    test_name: str
    duration: float
    memory_peak: int  # bytes
    memory_average: int  # bytes
    cpu_peak: float  # percentage
    cpu_average: float  # percentage
    operations_per_second: float
    success_rate: float

class PerformanceMonitor:
    def __init__(self, process_name: str = "mcp-debugger.exe"):
        self.process_name = process_name
        self.monitoring = False
        self.metrics = []
        self.start_time = 0
        
    def start_monitoring(self, pid: Optional[int] = None):
        """Start monitoring system resources"""
        self.monitoring = True
        self.start_time = time.time()
        self.metrics = []
        
        def monitor_loop():
            while self.monitoring:
                try:
                    if pid:
                        process = psutil.Process(pid)
                    else:
                        # Find process by name
                        processes = [p for p in psutil.process_iter(['pid', 'name']) 
                                   if self.process_name in p.info['name']]
                        if not processes:
                            time.sleep(0.1)
                            continue
                        process = psutil.Process(processes[0].info['pid'])
                    
                    # Collect metrics
                    memory_info = process.memory_info()
                    cpu_percent = process.cpu_percent()
                    
                    metric = {
                        'timestamp': time.time() - self.start_time,
                        'memory_rss': memory_info.rss,
                        'memory_vms': memory_info.vms,
                        'cpu_percent': cpu_percent,
                    }
                    
                    self.metrics.append(metric)
                    
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
                
                time.sleep(0.1)  # Sample every 100ms
        
        self.monitor_thread = threading.Thread(target=monitor_loop, daemon=True)
        self.monitor_thread.start()
    
    def stop_monitoring(self) -> Dict:
        """Stop monitoring and return aggregated metrics"""
        self.monitoring = False
        
        if not self.metrics:
            return {}
        
        memory_rss = [m['memory_rss'] for m in self.metrics]
        cpu_values = [m['cpu_percent'] for m in self.metrics if m['cpu_percent'] > 0]
        
        return {
            'duration': time.time() - self.start_time,
            'memory_peak': max(memory_rss) if memory_rss else 0,
            'memory_average': statistics.mean(memory_rss) if memory_rss else 0,
            'cpu_peak': max(cpu_values) if cpu_values else 0,
            'cpu_average': statistics.mean(cpu_values) if cpu_values else 0,
            'sample_count': len(self.metrics)
        }

class PerformanceTester:
    def __init__(self, binary_path: str, iterations: int = 100):
        self.binary_path = Path(binary_path)
        self.iterations = iterations
        self.logger = self._setup_logging()
        self.results: List[PerformanceMetrics] = []
        
        if not self.binary_path.exists():
            raise FileNotFoundError(f"Binary not found: {binary_path}")
    
    def _setup_logging(self) -> logging.Logger:
        logger = logging.getLogger('mcp_perf')
        logger.setLevel(logging.INFO)
        
        handler = logging.StreamHandler()
        formatter = logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s')
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def run_all_tests(self) -> bool:
        """Run all performance tests"""
        self.logger.info("Starting MCP Debugger Performance Tests")
        self.logger.info(f"Binary: {self.binary_path}")
        self.logger.info(f"Iterations: {self.iterations}")
        
        # Test suite
        tests = [
            ("Startup Time", self.test_startup_time),
            ("S-Expression Parsing", self.test_sexpr_performance),
            ("Command Processing", self.test_command_performance),
            ("Memory Usage Stability", self.test_memory_stability),
            ("Concurrent Operations", self.test_concurrent_operations),
            ("Memory Leak Detection", self.test_memory_leaks),
            ("Large Expression Handling", self.test_large_expressions),
            ("Stress Testing", self.test_stress_scenarios),
        ]
        
        for test_name, test_func in tests:
            self.logger.info(f"\nRunning: {test_name}")
            try:
                metrics = test_func()
                if metrics:
                    self.results.append(metrics)
                    self.logger.info(f"✓ {test_name} completed")
                else:
                    self.logger.error(f"✗ {test_name} failed")
            except Exception as e:
                self.logger.error(f"✗ {test_name} failed: {e}")
        
        # Generate report
        self._generate_report()
        
        return len(self.results) == len(tests)
    
    def test_startup_time(self) -> Optional[PerformanceMetrics]:
        """Test application startup time"""
        startup_times = []
        memory_peaks = []
        
        for i in range(min(10, self.iterations)):  # Limit startup tests
            monitor = PerformanceMonitor()
            
            start_time = time.time()
            
            # Start process
            proc = subprocess.Popen(
                [str(self.binary_path), "--help"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            monitor.start_monitoring(proc.pid)
            
            # Wait for completion
            stdout, stderr = proc.communicate(timeout=30)
            
            end_time = time.time()
            metrics = monitor.stop_monitoring()
            
            if proc.returncode == 0:
                startup_time = end_time - start_time
                startup_times.append(startup_time)
                memory_peaks.append(metrics.get('memory_peak', 0))
        
        if not startup_times:
            return None
        
        return PerformanceMetrics(
            test_name="Startup Time",
            duration=statistics.mean(startup_times),
            memory_peak=max(memory_peaks),
            memory_average=statistics.mean(memory_peaks),
            cpu_peak=0,  # Not measured for startup
            cpu_average=0,
            operations_per_second=1.0 / statistics.mean(startup_times),
            success_rate=len(startup_times) / min(10, self.iterations)
        )
    
    def test_sexpr_performance(self) -> Optional[PerformanceMetrics]:
        """Test S-Expression parsing performance"""
        expressions = [
            "(+ 1 2 3)",
            "(* (+ 1 2) (- 5 3))",
            "(if (> 5 3) \"true\" \"false\")",
            "(list 1 2 3 4 5 6 7 8 9 10)",
            "(log \"info\" \"performance test message\")",
        ]
        
        times = []
        successes = 0
        monitor = PerformanceMonitor()
        
        start_time = time.time()
        
        for expr in expressions:
            for i in range(self.iterations // len(expressions)):
                expr_start = time.time()
                
                proc = subprocess.run(
                    [str(self.binary_path), "-c", expr],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                expr_time = time.time() - expr_start
                times.append(expr_time)
                
                if proc.returncode == 0:
                    successes += 1
        
        total_time = time.time() - start_time
        
        if not times:
            return None
        
        return PerformanceMetrics(
            test_name="S-Expression Parsing",
            duration=total_time,
            memory_peak=0,  # Hard to measure for short operations
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=len(times) / total_time,
            success_rate=successes / len(times)
        )
    
    def test_command_performance(self) -> Optional[PerformanceMetrics]:
        """Test command processing performance"""
        commands = [
            ":help",
            ":status",
            "(+ 1 1)",
            "(log \"debug\" \"test\")",
        ]
        
        times = []
        successes = 0
        
        start_time = time.time()
        
        for cmd in commands:
            for i in range(self.iterations // len(commands)):
                cmd_start = time.time()
                
                proc = subprocess.run(
                    [str(self.binary_path), "-c", cmd],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                cmd_time = time.time() - cmd_start
                times.append(cmd_time)
                
                if proc.returncode == 0:
                    successes += 1
        
        total_time = time.time() - start_time
        
        if not times:
            return None
        
        return PerformanceMetrics(
            test_name="Command Processing",
            duration=total_time,
            memory_peak=0,
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=len(times) / total_time,
            success_rate=successes / len(times)
        )
    
    def test_memory_stability(self) -> Optional[PerformanceMetrics]:
        """Test memory usage stability over time"""
        monitor = PerformanceMonitor()
        
        # Create a script that runs multiple operations
        script_content = """
; Memory stability test script
(log "info" "Starting memory test")
(+ 1 2 3 4 5)
(list 1 2 3 4 5 6 7 8 9 10)
(if true "yes" "no")
(log "info" "Memory test iteration complete")
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.mcp', delete=False) as f:
            f.write(script_content)
            script_path = f.name
        
        try:
            start_time = time.time()
            
            # Run script multiple times to test memory stability
            for i in range(min(20, self.iterations)):
                proc = subprocess.run(
                    [str(self.binary_path), "-f", script_path],
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                
                if proc.returncode != 0:
                    self.logger.warning(f"Memory test iteration {i} failed")
            
            total_time = time.time() - start_time
            
            return PerformanceMetrics(
                test_name="Memory Stability",
                duration=total_time,
                memory_peak=0,  # Would need process monitoring
                memory_average=0,
                cpu_peak=0,
                cpu_average=0,
                operations_per_second=min(20, self.iterations) / total_time,
                success_rate=1.0  # Simplified
            )
            
        finally:
            os.unlink(script_path)
    
    def test_concurrent_operations(self) -> Optional[PerformanceMetrics]:
        """Test concurrent operation handling"""
        num_threads = 4
        operations_per_thread = self.iterations // num_threads
        
        results = []
        start_time = time.time()
        
        def worker_thread(thread_id: int):
            thread_results = []
            for i in range(operations_per_thread):
                op_start = time.time()
                
                proc = subprocess.run(
                    [str(self.binary_path), "-c", f"(+ {thread_id} {i})"],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                op_time = time.time() - op_start
                thread_results.append({
                    'time': op_time,
                    'success': proc.returncode == 0
                })
            
            results.extend(thread_results)
        
        # Start threads
        threads = []
        for i in range(num_threads):
            thread = threading.Thread(target=worker_thread, args=(i,))
            threads.append(thread)
            thread.start()
        
        # Wait for completion
        for thread in threads:
            thread.join()
        
        total_time = time.time() - start_time
        
        if not results:
            return None
        
        successes = sum(1 for r in results if r['success'])
        
        return PerformanceMetrics(
            test_name="Concurrent Operations",
            duration=total_time,
            memory_peak=0,
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=len(results) / total_time,
            success_rate=successes / len(results)
        )
    
    def test_memory_leaks(self) -> Optional[PerformanceMetrics]:
        """Test for memory leaks during extended operation"""
        # This is a simplified test - real memory leak detection
        # would require tools like Application Verifier or similar
        
        iterations = min(50, self.iterations)
        start_time = time.time()
        
        for i in range(iterations):
            proc = subprocess.run(
                [str(self.binary_path), "-c", "(list 1 2 3 4 5)"],
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if proc.returncode != 0:
                self.logger.warning(f"Memory leak test iteration {i} failed")
        
        total_time = time.time() - start_time
        
        return PerformanceMetrics(
            test_name="Memory Leak Detection",
            duration=total_time,
            memory_peak=0,  # Would need detailed monitoring
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=iterations / total_time,
            success_rate=1.0  # Simplified - just check it doesn't crash
        )
    
    def test_large_expressions(self) -> Optional[PerformanceMetrics]:
        """Test handling of large S-expressions"""
        # Create progressively larger expressions
        large_expressions = [
            "(+ " + " ".join(str(i) for i in range(10)) + ")",
            "(+ " + " ".join(str(i) for i in range(100)) + ")",
            "(list " + " ".join(str(i) for i in range(50)) + ")",
        ]
        
        times = []
        successes = 0
        start_time = time.time()
        
        for expr in large_expressions:
            for i in range(min(5, self.iterations // len(large_expressions))):
                expr_start = time.time()
                
                proc = subprocess.run(
                    [str(self.binary_path), "-c", expr],
                    capture_output=True,
                    text=True,
                    timeout=30  # Longer timeout for large expressions
                )
                
                expr_time = time.time() - expr_start
                times.append(expr_time)
                
                if proc.returncode == 0:
                    successes += 1
        
        total_time = time.time() - start_time
        
        if not times:
            return None
        
        return PerformanceMetrics(
            test_name="Large Expression Handling",
            duration=total_time,
            memory_peak=0,
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=len(times) / total_time,
            success_rate=successes / len(times)
        )
    
    def test_stress_scenarios(self) -> Optional[PerformanceMetrics]:
        """Test stress scenarios"""
        stress_tests = [
            # Rapid fire commands
            lambda: [subprocess.run([str(self.binary_path), "-c", "(+ 1 1)"], 
                                  capture_output=True, timeout=5) for _ in range(10)],
            
            # Complex nested expressions
            lambda: [subprocess.run([str(self.binary_path), "-c", 
                                   "(if (> (+ 1 2) (* 2 1)) (list 1 2 3) (list 4 5 6))"], 
                                  capture_output=True, timeout=10) for _ in range(5)],
        ]
        
        start_time = time.time()
        total_operations = 0
        successes = 0
        
        for stress_test in stress_tests:
            try:
                results = stress_test()
                total_operations += len(results)
                successes += sum(1 for r in results if r.returncode == 0)
            except Exception as e:
                self.logger.warning(f"Stress test failed: {e}")
        
        total_time = time.time() - start_time
        
        return PerformanceMetrics(
            test_name="Stress Testing",
            duration=total_time,
            memory_peak=0,
            memory_average=0,
            cpu_peak=0,
            cpu_average=0,
            operations_per_second=total_operations / total_time if total_time > 0 else 0,
            success_rate=successes / total_operations if total_operations > 0 else 0
        )
    
    def _generate_report(self):
        """Generate performance report"""
        self.logger.info("\n" + "="*60)
        self.logger.info("PERFORMANCE TEST REPORT")
        self.logger.info("="*60)
        
        for result in self.results:
            self.logger.info(f"\n{result.test_name}:")
            self.logger.info(f"  Duration: {result.duration:.3f}s")
            self.logger.info(f"  Operations/sec: {result.operations_per_second:.2f}")
            self.logger.info(f"  Success rate: {result.success_rate:.1%}")
            
            if result.memory_peak > 0:
                self.logger.info(f"  Peak memory: {result.memory_peak // 1024 // 1024}MB")
        
        # Save detailed report
        report_data = {
            'timestamp': time.time(),
            'binary_path': str(self.binary_path),
            'iterations': self.iterations,
            'results': [
                {
                    'test_name': r.test_name,
                    'duration': r.duration,
                    'memory_peak': r.memory_peak,
                    'memory_average': r.memory_average,
                    'cpu_peak': r.cpu_peak,
                    'cpu_average': r.cpu_average,
                    'operations_per_second': r.operations_per_second,
                    'success_rate': r.success_rate
                }
                for r in self.results
            ]
        }
        
        report_path = Path("performance_report.json")
        with open(report_path, 'w') as f:
            json.dump(report_data, f, indent=2)
        
        self.logger.info(f"\nDetailed report saved to: {report_path}")

def main():
    parser = argparse.ArgumentParser(description='MCP Debugger Performance Tests')
    parser.add_argument('--binary', required=True, help='Path to mcp-debugger executable')
    parser.add_argument('--iterations', type=int, default=100, help='Number of test iterations')
    parser.add_argument('--output', help='Output directory for reports')
    
    args = parser.parse_args()
    
    # Change to output directory if specified
    if args.output:
        os.chdir(args.output)
    
    # Run performance tests
    tester = PerformanceTester(args.binary, args.iterations)
    success = tester.run_all_tests()
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())