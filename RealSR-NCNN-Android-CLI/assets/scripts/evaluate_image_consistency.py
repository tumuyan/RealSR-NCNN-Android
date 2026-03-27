import os
import sys
import json
import numpy as np
import cv2
from datetime import datetime
from pathlib import Path
from skimage.metrics import structural_similarity as ssim
from scipy import ndimage
from scipy.stats import entropy


class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        elif isinstance(obj, np.floating):
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        return super().default(obj)


class ImageAnomalyDetector:
    def __init__(self, input_dir, output_dir):
        self.input_dir = Path(input_dir)
        self.output_dir = Path(output_dir)
        self.results = []
        
    def load_image(self, path):
        img = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
        if img is None:
            raise ValueError(f"Failed to load image: {path}")
        
        if img.shape[2] == 4:
            bgr = img[:, :, :3]
            alpha = img[:, :, 3].astype(np.float32) / 255.0
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        else:
            rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
            alpha = np.ones(rgb.shape[:2], dtype=np.float32)
        
        return rgb, alpha
    
    def resize_image(self, img, target_size):
        if len(img.shape) == 2:
            return cv2.resize(img, (target_size[1], target_size[0]), 
                            interpolation=cv2.INTER_AREA)
        else:
            return cv2.resize(img, (target_size[1], target_size[0]), 
                            interpolation=cv2.INTER_AREA)
    
    def detect_alpha_anomalies(self, input_alpha, output_alpha):
        h, w = output_alpha.shape
        input_alpha_resized = self.resize_image(input_alpha, (h, w))
        
        anomalies = {}
        
        alpha_diff = np.abs(output_alpha - input_alpha_resized)
        
        opaque_mask = input_alpha_resized > 0.8
        transparent_mask = output_alpha < 0.3
        sudden_transparency = opaque_mask & transparent_mask
        
        sudden_transparency_ratio = np.sum(sudden_transparency) / (h * w)
        anomalies['sudden_transparency_ratio'] = sudden_transparency_ratio
        
        if sudden_transparency_ratio > 0.05:
            anomalies['sudden_transparency_severity'] = 'severe'
        elif sudden_transparency_ratio > 0.01:
            anomalies['sudden_transparency_severity'] = 'moderate'
        else:
            anomalies['sudden_transparency_severity'] = 'none'
        
        input_edges = cv2.Canny((input_alpha_resized * 255).astype(np.uint8), 100, 200)
        output_edges = cv2.Canny((output_alpha * 255).astype(np.uint8), 100, 200)
        
        edge_diff = np.abs(input_edges.astype(np.float32) - output_edges.astype(np.float32))
        edge_change_ratio = np.sum(edge_diff > 50) / (h * w)
        anomalies['edge_change_ratio'] = edge_change_ratio
        
        input_hist, _ = np.histogram(input_alpha_resized, bins=50, range=(0, 1), density=True)
        output_hist, _ = np.histogram(output_alpha, bins=50, range=(0, 1), density=True)
        
        hist_diff = np.abs(input_hist - output_hist)
        hist_divergence = np.sum(hist_diff)
        anomalies['histogram_divergence'] = hist_divergence
        
        input_entropy = entropy(input_hist + 1e-10)
        output_entropy = entropy(output_hist + 1e-10)
        entropy_change = abs(input_entropy - output_entropy)
        anomalies['entropy_change'] = entropy_change
        
        alpha_anomaly_score = (
            sudden_transparency_ratio * 1000 +
            edge_change_ratio * 500 +
            hist_divergence * 10 +
            entropy_change * 20
        )
        anomalies['alpha_anomaly_score'] = min(alpha_anomaly_score, 100)
        
        return anomalies
    
    def detect_rgb_anomalies(self, input_rgb, output_rgb, input_alpha, output_alpha):
        h, w = output_rgb.shape[:2]
        input_rgb_resized = self.resize_image(input_rgb, (h, w))
        
        anomalies = {}
        
        for i, channel in enumerate(['R', 'G', 'B']):
            input_ch = input_rgb_resized[:, :, i]
            output_ch = output_rgb[:, :, i]
            
            mean_diff = abs(np.mean(input_ch) - np.mean(output_ch))
            std_diff = abs(np.std(input_ch) - np.std(output_ch))
            
            anomalies[f'{channel}_mean_diff'] = mean_diff
            anomalies[f'{channel}_std_diff'] = std_diff
        
        input_mean = np.mean(input_rgb_resized, axis=(0, 1))
        output_mean = np.mean(output_rgb, axis=(0, 1))
        
        input_ratio = input_mean / (np.sum(input_mean) + 1e-10)
        output_ratio = output_mean / (np.sum(output_mean) + 1e-10)
        
        ratio_diff = np.abs(input_ratio - output_ratio)
        max_ratio_diff = np.max(ratio_diff)
        anomalies['max_channel_ratio_diff'] = max_ratio_diff
        
        if max_ratio_diff > 0.3:
            anomalies['channel_ratio_severity'] = 'severe'
        elif max_ratio_diff > 0.15:
            anomalies['channel_ratio_severity'] = 'moderate'
        else:
            anomalies['channel_ratio_severity'] = 'none'
        
        input_gray = cv2.cvtColor(input_rgb_resized, cv2.COLOR_RGB2GRAY)
        output_gray = cv2.cvtColor(output_rgb, cv2.COLOR_RGB2GRAY)
        
        ssim_score = ssim(input_gray, output_gray, data_range=255)
        anomalies['ssim_score'] = ssim_score
        
        if ssim_score < 0.7:
            anomalies['ssim_severity'] = 'severe'
        elif ssim_score < 0.85:
            anomalies['ssim_severity'] = 'moderate'
        else:
            anomalies['ssim_severity'] = 'none'
        
        mse = np.mean((input_rgb_resized.astype(np.float32) - output_rgb.astype(np.float32)) ** 2)
        anomalies['mse'] = mse
        
        if mse > 0:
            psnr = 10 * np.log10(255 ** 2 / mse)
            anomalies['psnr'] = psnr
        else:
            anomalies['psnr'] = 100
        
        input_alpha_resized = self.resize_image(input_alpha, (h, w))
        opaque_mask = (input_alpha_resized > 0.5) & (output_alpha > 0.5)
        
        if np.sum(opaque_mask) > 0:
            input_opaque = input_rgb_resized[opaque_mask]
            output_opaque = output_rgb[opaque_mask]
            
            if len(input_opaque) > 0:
                input_opaque_gray = cv2.cvtColor(input_opaque.reshape(-1, 1, 3), cv2.COLOR_RGB2GRAY).flatten()
                output_opaque_gray = cv2.cvtColor(output_opaque.reshape(-1, 1, 3), cv2.COLOR_RGB2GRAY).flatten()
                
                if len(input_opaque_gray) > 10:
                    ssim_opaque = ssim(
                        input_opaque_gray.reshape(-1), 
                        output_opaque_gray.reshape(-1), 
                        data_range=255
                    )
                    anomalies['ssim_opaque'] = ssim_opaque
        
        laplacian_input = cv2.Laplacian(input_gray, cv2.CV_64F)
        laplacian_output = cv2.Laplacian(output_gray, cv2.CV_64F)
        
        noise_input = np.std(laplacian_input)
        noise_output = np.std(laplacian_output)
        noise_ratio = noise_output / (noise_input + 1e-10)
        anomalies['noise_ratio'] = noise_ratio
        
        if noise_ratio > 2.0:
            anomalies['noise_severity'] = 'severe'
        elif noise_ratio > 1.5:
            anomalies['noise_severity'] = 'moderate'
        else:
            anomalies['noise_severity'] = 'none'
        
        rgb_anomaly_score = (
            max_ratio_diff * 100 +
            (1 - ssim_score) * 50 +
            min(mse / 1000, 1) * 20 +
            abs(1 - noise_ratio) * 30
        )
        anomalies['rgb_anomaly_score'] = min(rgb_anomaly_score, 100)
        
        return anomalies
    
    def detect_edge_anomalies(self, input_rgb, output_rgb, input_alpha, output_alpha):
        h, w = output_rgb.shape[:2]
        input_rgb_resized = self.resize_image(input_rgb, (h, w))
        input_alpha_resized = self.resize_image(input_alpha, (h, w))
        
        anomalies = {}
        border = 4
        
        # Create edge mask: 4px border on all sides
        edge_mask = np.zeros((h, w), dtype=bool)
        edge_mask[:border, :] = True
        edge_mask[-border:, :] = True
        edge_mask[:, :border] = True
        edge_mask[:, -border:] = True
        
        # Extract edge pixels for RGB using mask
        input_edge_rgb = input_rgb_resized[edge_mask].astype(np.float32)  # (N, 3)
        output_edge_rgb = output_rgb[edge_mask].astype(np.float32)
        
        # Edge RGB metrics
        edge_rgb_mae = np.mean(np.abs(input_edge_rgb - output_edge_rgb))
        edge_rgb_mse = np.mean((input_edge_rgb - output_edge_rgb) ** 2)
        anomalies['edge_rgb_mae'] = edge_rgb_mae
        anomalies['edge_rgb_mse'] = edge_rgb_mse
        
        # Per-channel edge MAE
        for i, channel in enumerate(['R', 'G', 'B']):
            anomalies[f'edge_{channel.lower()}_mae'] = np.mean(np.abs(input_edge_rgb[:, i] - output_edge_rgb[:, i]))
        
        # Extract edge pixels for Alpha using mask
        input_edge_alpha = input_alpha_resized[edge_mask].astype(np.float32)  # (N,)
        output_edge_alpha = output_alpha[edge_mask].astype(np.float32)
        
        # Edge Alpha metrics
        edge_alpha_mae = np.mean(np.abs(input_edge_alpha - output_edge_alpha))
        edge_alpha_mse = np.mean((input_edge_alpha - output_edge_alpha) ** 2)
        anomalies['edge_alpha_mae'] = edge_alpha_mae
        anomalies['edge_alpha_mse'] = edge_alpha_mse
        
        # Edge RGB severity and score
        edge_rgb_score = min(edge_rgb_mae * 2, 100)
        if edge_rgb_score > 30:
            anomalies['edge_rgb_severity'] = 'severe'
        elif edge_rgb_score > 15:
            anomalies['edge_rgb_severity'] = 'moderate'
        elif edge_rgb_score > 5:
            anomalies['edge_rgb_severity'] = 'mild'
        else:
            anomalies['edge_rgb_severity'] = 'normal'
        anomalies['edge_rgb_score'] = edge_rgb_score
        
        # Edge Alpha severity and score
        edge_alpha_score = min(edge_alpha_mae * 200, 100)
        if edge_alpha_score > 30:
            anomalies['edge_alpha_severity'] = 'severe'
        elif edge_alpha_score > 15:
            anomalies['edge_alpha_severity'] = 'moderate'
        elif edge_alpha_score > 5:
            anomalies['edge_alpha_severity'] = 'mild'
        else:
            anomalies['edge_alpha_severity'] = 'normal'
        anomalies['edge_alpha_score'] = edge_alpha_score
        
        return anomalies
    
    def calculate_overall_score(self, alpha_anomalies, rgb_anomalies):
        alpha_score = alpha_anomalies.get('alpha_anomaly_score', 0)
        rgb_score = rgb_anomalies.get('rgb_anomaly_score', 0)
        
        overall_score = alpha_score * 0.6 + rgb_score * 0.4
        
        if overall_score > 60:
            severity = 'severe'
        elif overall_score > 30:
            severity = 'moderate'
        elif overall_score > 10:
            severity = 'mild'
        else:
            severity = 'normal'
        
        return {
            'overall_score': overall_score,
            'severity': severity
        }
    
    def parse_output_filename(self, filename):
        stem = filename.stem
        
        if '_' not in stem:
            return '', '', stem
        
        parts = stem.split('_')
        if len(parts) < 2:
            return '', '', stem
        
        input_name = parts[-1]
        program = parts[0]
        params = '_'.join(parts[1:-1]) if len(parts) > 2 else ''
        
        return program, params, input_name
    
    def extract_inference_program(self, program):
        return program if program else 'unknown'
    
    def match_files(self):
        input_files = list(self.input_dir.glob('*.png')) + list(self.input_dir.glob('*.jpg'))
        output_files = list(self.output_dir.glob('*.png')) + list(self.output_dir.glob('*.jpg'))
        
        matches = []
        for output_file in output_files:
            program, params, input_name = self.parse_output_filename(output_file)
            input_file = self.input_dir / f"{input_name}{output_file.suffix}"
            
            if input_file.exists():
                matches.append({
                    'input_file': input_file,
                    'output_file': output_file,
                    'program': program,
                    'params': params,
                    'input_name': input_name,
                    'has_output': True
                })
        
        return matches
    
    def evaluate(self, csv_data=None):
        if csv_data:
            # 如果提供了CSV文件，只分析CSV文件中指定的output图像
            print("Using CSV file as reference, only evaluating specified output images")
            
            # 收集所有输出文件
            output_files = list(self.output_dir.glob('*.png')) + list(self.output_dir.glob('*.jpg'))
            output_file_map = {f.name: f for f in output_files}
            
            # 处理CSV文件中的记录
            for key, value in csv_data.items():
                if isinstance(value, dict):
                    output_filename = value.get('output_filename')
                    if output_filename and output_filename in output_file_map:
                        # 找到对应的输出文件
                        output_file = output_file_map[output_filename]
                        
                        # 解析输出文件名，提取输入文件名
                        _, _, input_name = self.parse_output_filename(output_file)
                        input_file = self.input_dir / f"{input_name}{output_file.suffix}"
                        
                        if input_file.exists():
                            try:
                                input_rgb, input_alpha = self.load_image(input_file)
                                output_rgb, output_alpha = self.load_image(output_file)
                                
                                alpha_anomalies = self.detect_alpha_anomalies(input_alpha, output_alpha)
                                rgb_anomalies = self.detect_rgb_anomalies(input_rgb, output_rgb, input_alpha, output_alpha)
                                edge_anomalies = self.detect_edge_anomalies(input_rgb, output_rgb, input_alpha, output_alpha)
                                overall = self.calculate_overall_score(alpha_anomalies, rgb_anomalies)
                                
                                result = {
                                    'output_filename': output_file.name,
                                    'input_filename': input_file.name,
                                    'params': value.get('params', ''),
                                    'program': value.get('program_name', ''),
                                    'input_name': input_name,
                                    'output_size': output_rgb.shape[:2],
                                    'input_size': input_rgb.shape[:2],
                                    'alpha_anomalies': alpha_anomalies,
                                    'rgb_anomalies': rgb_anomalies,
                                    'edge_anomalies': edge_anomalies,
                                    'overall': overall,
                                    'has_output': True
                                }
                                
                                self.results.append(result)
                                print(f"Evaluated: {output_file.name} - Severity: {overall['severity']}")
                                
                            except Exception as e:
                                print(f"Error processing {output_file.name}: {e}")
                                continue
                    else:
                        # 添加没有输出的记录
                        result = {
                            'output_filename': value.get('output_filename', ''),
                            'input_filename': value.get('input_filename', ''),
                            'params': value.get('params', ''),
                            'program': value.get('program_name', ''),
                            'input_name': value.get('input_filename', ''),
                            'output_size': None,
                            'input_size': None,
                            'alpha_anomalies': None,
                            'rgb_anomalies': None,
                            'edge_anomalies': None,
                            'overall': {
                                'overall_score': 0,
                                'severity': 'none'
                            },
                            'has_output': False
                        }
                        self.results.append(result)
                        if not value.get('output_filename'):
                            print(f"Added empty record: {value.get('input_filename', 'unknown')}")
        else:
            # 如果没有提供CSV文件，分析output目录的所有图片
            print("No CSV file provided, evaluating all images in output directory")
            
            matches = self.match_files()
            
            for match in matches:
                try:
                    input_rgb, input_alpha = self.load_image(match['input_file'])
                    output_rgb, output_alpha = self.load_image(match['output_file'])
                    
                    alpha_anomalies = self.detect_alpha_anomalies(input_alpha, output_alpha)
                    rgb_anomalies = self.detect_rgb_anomalies(input_rgb, output_rgb, input_alpha, output_alpha)
                    edge_anomalies = self.detect_edge_anomalies(input_rgb, output_rgb, input_alpha, output_alpha)
                    overall = self.calculate_overall_score(alpha_anomalies, rgb_anomalies)
                    
                    result = {
                        'output_filename': match['output_file'].name,
                        'input_filename': match['input_file'].name,
                        'params': match['params'],
                        'program': match['program'],
                        'input_name': match['input_name'],
                        'output_size': output_rgb.shape[:2],
                        'input_size': input_rgb.shape[:2],
                        'alpha_anomalies': alpha_anomalies,
                        'rgb_anomalies': rgb_anomalies,
                        'edge_anomalies': edge_anomalies,
                        'overall': overall,
                        'has_output': True
                    }
                    
                    self.results.append(result)
                    print(f"Evaluated: {match['output_file'].name} - Severity: {overall['severity']}")
                    
                except Exception as e:
                    print(f"Error processing {match['output_file'].name}: {e}")
                    continue
        
        return self.results
    
    def generate_html_report(self, output_path, csv_data=None):
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        report_filename = f"evaluation_report_{timestamp}.html"
        
        if csv_data and 'csv_file' in csv_data and csv_data['csv_file']:
            csv_path = Path(csv_data['csv_file'])
            if csv_path.exists():
                report_filename = csv_path.stem + '.html'
        
        report_path = Path(output_path) / report_filename
        
        report_path.parent.mkdir(parents=True, exist_ok=True)
        
        template_path = Path(__file__).parent / 'report_viewer2.html'
        if not template_path.exists():
            print(f"Warning: Template file not found: {template_path}")
            return None
        
        with open(template_path, 'r', encoding='utf-8') as f:
            template_content = f.read()
        
        js_filename = report_filename.replace('.html', '.js')
        html_content = template_content.replace('<script data-js-path="">', f'<script data-js-path="{js_filename}">')
        
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"HTML report generated: {report_path}")
        return report_path
    
    def generate_html_report(self, output_path, csv_data=None):
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        report_filename = f"evaluation_report_{timestamp}.html"
        
        if csv_data and 'csv_file' in csv_data and csv_data['csv_file']:
            csv_path = Path(csv_data['csv_file'])
            if csv_path.exists():
                report_filename = csv_path.stem + '.html'
        
        report_path = Path(output_path) / report_filename
        
        report_path.parent.mkdir(parents=True, exist_ok=True)
        
        template_path = Path(__file__).parent / 'report_viewer2.html'
        if not template_path.exists():
            print(f"Warning: Template file not found: {template_path}")
            return None
        
        with open(template_path, 'r', encoding='utf-8') as f:
            template_content = f.read()
        
        js_filename = report_filename.replace('.html', '.js')
        html_content = template_content.replace('<script data-js-path="">', f'<script data-js-path="{js_filename}">')
        
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"HTML report generated: {report_path}")
        return report_path
    
    def generate_js_report(self, output_path, csv_data=None):
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        js_filename = f"evaluation_report_{timestamp}.js"
        
        if csv_data and 'csv_file' in csv_data and csv_data['csv_file']:
            csv_path = Path(csv_data['csv_file'])
            if csv_path.exists():
                js_filename = csv_path.stem + '.js'
        
        js_path = Path(output_path) / js_filename
        js_path.parent.mkdir(parents=True, exist_ok=True)
        
        severity_counts = {'normal': 0, 'mild': 0, 'moderate': 0, 'severe': 0, 'none': 0}
        for result in self.results:
            severity_counts[result['overall']['severity']] += 1
        
        csv_stats = None
        if csv_data:
            csv_keys = set()
            csv_empty_outputs = 0
            csv_file_name = Path(csv_data['csv_file']).name if csv_data and 'csv_file' in csv_data and csv_data['csv_file'] else ''
            
            for key, value in csv_data.items():
                if isinstance(value, dict):
                    csv_keys.add(value['output_filename'])
                    if not value['output_filename']:
                        csv_empty_outputs += 1
            
            matched = 0
            unmatched = 0
            test_count = 0
            empty_count = 0
            
            for result in self.results:
                if result['output_filename'] in csv_keys:
                    matched += 1
                    if not result.get('has_output', True):
                        empty_count += 1
                    else:
                        test_count += 1
                else:
                    unmatched += 1
            
            csv_stats = {
                'csv_file': csv_file_name,
                'total_csv': len(csv_data) - 1,
                'matched': matched,
                'unmatched': unmatched,
                'test': test_count,
                'empty': empty_count
            }
        
        report_data = {
            'metadata': {
                'timestamp': timestamp,
                'total_files': len(self.results),
                'csv_file': csv_data.get('csv_file', '') if csv_data else ''
            },
            'severity_counts': severity_counts,
            'csv_stats': csv_stats,
            'results': self.results
        }
        
        with open(js_path, 'w', encoding='utf-8') as f:
            f.write('window.reportData = ')
            json.dump(report_data, f, indent=2, ensure_ascii=False, cls=NumpyEncoder)
            f.write(';')
        
        print(f"JS report generated: {js_path}")
        return js_path
    

def main():
    script_dir = Path(__file__).parent
    assets_dir = script_dir.parent
    input_dir = assets_dir / 'input'
    output_dir = assets_dir / 'output'
    report_dir = assets_dir / 'report'
    
    csv_file = None
    csv_data = {}
    
    if len(sys.argv) > 1:
        csv_file = sys.argv[1]
        csv_path = Path(csv_file)
        if not csv_path.is_absolute():
            csv_path = Path.cwd().parent / 'report' / csv_file
        
        if csv_path.exists():
            print(f"Loading CSV file: {csv_path}")
            csv_data['csv_file'] = str(csv_path)
            with open(csv_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('input_filename'):
                        parts = line.split(',')
                        if len(parts) >= 5:
                            input_filename = parts[0].strip()
                            program_name = parts[1].strip()
                            param_group = parts[2].strip()
                            params = parts[3].strip()
                            output_filename = parts[4].strip()
                            key = f"{input_filename}_{program_name}_{param_group}"
                            csv_data[key] = {
                                'input_filename': input_filename,
                                'program_name': program_name,
                                'param_group': param_group,
                                'params': params,
                                'output_filename': output_filename
                            }
            print(f"Loaded {len(csv_data) - 1} records from CSV")
        else:
            print(f"Warning: CSV file not found: {csv_path}")
    
    if not input_dir.exists():
        print(f"Error: Input directory not found: {input_dir}")
        sys.exit(1)
    
    if not output_dir.exists():
        print(f"Error: Output directory not found: {output_dir}")
        sys.exit(1)
    
    print("Starting image anomaly detection...")
    print(f"Input directory: {input_dir}")
    print(f"Output directory: {output_dir}")
    if csv_file:
        print(f"CSV reference file: {csv_file}")
    print()
    
    detector = ImageAnomalyDetector(input_dir, output_dir)
    results = detector.evaluate(csv_data)
    
    print(f"\nEvaluation completed. Total files processed: {len(results)}")
    
    js_path = detector.generate_js_report(report_dir, csv_data)
    print(f"\nJS report saved to: {js_path}")
    
    report_path = detector.generate_html_report(report_dir, csv_data)
    print(f"HTML report saved to: {report_path}")
    
    severity_counts = {'normal': 0, 'mild': 0, 'moderate': 0, 'severe': 0, 'none': 0}
    for result in results:
        severity_counts[result['overall']['severity']] += 1
    
    print("\nSummary:")
    print(f"  Normal: {severity_counts['normal']}")
    print(f"  Mild: {severity_counts['mild']}")
    print(f"  Moderate: {severity_counts['moderate']}")
    print(f"  Severe: {severity_counts['severe']}")


if __name__ == '__main__':
    main()
