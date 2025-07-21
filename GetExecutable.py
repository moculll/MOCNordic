import requests
import os
import subprocess
import argparse
import time
from tqdm import tqdm

GITHUB_USERNAME = 'Enter your github name'
GITHUB_REPO = 'Enter your github repo name'
GITHUB_TOKEN = 'Enter your github token'

API_URL = f'https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/actions/runs'

def get_latest_successful_run(branch):
    headers = {
        'Authorization': f'token {GITHUB_TOKEN}',
        'Accept': 'application/vnd.github.v3+json'
    }
    
    params = {
        'branch': branch,
        'event': 'push',
        'status': 'success',
        'per_page': 1
    }
    response = requests.get(API_URL, headers=headers, params=params)
    
    if response.status_code == 200:
        runs = response.json().get('workflow_runs', [])
        if runs:
            return runs[0]['id'], runs[0]['head_sha']
        else:
            print(f"No successful workflow runs found for branch '{branch}'")
    else:
        print(f'Failed to get workflow: {response.status_code}')
    
    return None, None

def get_remote_commit_hash(branch):
    headers = {
        'Authorization': f'token {GITHUB_TOKEN}',
        'Accept': 'application/vnd.github.v3+json'
    }
    
    url = f"https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/branches/{branch}"
    response = requests.get(url, headers=headers)
    
    if response.status_code == 200:
        return response.json()['commit']['sha']
    else:
        print(f"Failed to get remote commit hash: {response.status_code}")
        return None

def get_artifacts_url(run_id):
    headers = {
        'Authorization': f'token {GITHUB_TOKEN}',
        'Accept': 'application/vnd.github.v3+json'
    }
    
    artifacts_url = f'https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/actions/runs/{run_id}/artifacts'
    response = requests.get(artifacts_url, headers=headers)
    
    if response.status_code == 200:
        artifacts = response.json().get('artifacts', [])
        if artifacts:
            return artifacts[0]['archive_download_url']
        else:
            print("No artifacts found")
    else:
        print(f'Failed to get artifact list: {response.status_code}')
    
    return None
def download_artifact(download_url, save_name):
    """download artifact to ./executable/save_name"""
    headers = {
        'Authorization': f'token {GITHUB_TOKEN}'
    }
    
    os.makedirs("executable", exist_ok=True)
    save_path = os.path.join("executable", save_name)
    
    response = requests.get(download_url, headers=headers, stream=True)
    
    if response.status_code == 200:
        with open(save_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        print(f'✅ Downloaded artifact to: {save_path}')
        return save_path
    else:
        print(f'❌ Download failed: {response.status_code}')
        return None
def get_latest_workflow_status(branch):
    headers = {
        'Authorization': f'token {GITHUB_TOKEN}',
        'Accept': 'application/vnd.github.v3+json'
    }

    commit_url = f"https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/branches/{branch}"
    commit_resp = requests.get(commit_url, headers=headers)
    
    if commit_resp.status_code != 200:
        print(f"Error getting branch info: {commit_resp.status_code}")
        return False, None

    latest_commit_sha = commit_resp.json()['commit']['sha']

    runs_url = f"https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/actions/runs"
    params = {'branch': branch, 'per_page': 1}
    runs_resp = requests.get(runs_url, headers=headers, params=params)

    if runs_resp.status_code != 200:
        print(f"Error getting workflow runs: {runs_resp.status_code}")
        return False, None

    workflow_data = runs_resp.json()
    
    if not workflow_data['workflow_runs']:
        print("No workflow runs found but new commit exists - assuming ongoing")
        return True, None

    latest_run = workflow_data['workflow_runs'][0]
    run_id = latest_run['id']
    
    if latest_run['head_sha'] != latest_commit_sha:
        print(f"Latest workflow run doesn't match latest commit - assuming ongoing")
        return True, None
    
    if latest_run['status'] == "in_progress":
        print(f"Workflow for {latest_commit_sha[:8]} is running")
        return True, run_id
    
    print(f"Latest workflow status: {latest_run['status']} (conclusion: {latest_run.get('conclusion', 'none')})")
    return False, run_id

def monitor_workflow_progress(run_id, branch):

    headers = {
        'Authorization': f'token {GITHUB_TOKEN}',
        'Accept': 'application/vnd.github.v3+json'
    }
    
    print("\nMonitoring workflow progress...")
    with tqdm(total=100, desc="Workflow Progress") as pbar:
        while True:
            run_url = f"https://api.github.com/repos/{GITHUB_USERNAME}/{GITHUB_REPO}/actions/runs/{run_id}"
            response = requests.get(run_url, headers=headers)
            
            if response.status_code != 200:
                print(f"Error getting workflow status: {response.status_code}")
                return False
            
            run_data = response.json()
            status = run_data['status']
            
            if status == "completed":
                pbar.n = 100
                pbar.refresh()
                print("\nWorkflow completed!")
                return run_data['conclusion'] == "success"
            
            if pbar.n < 40:
                pbar.update(3)
            elif pbar.n < 80:
                pbar.update(1)
            elif pbar.n < 95:
                pbar.update(2)
            elif pbar.n < 99:
                pbar.update(1)
            else:
                pbar.update(0)
            time.sleep(1)
            

def prompt_and_wait(branch):

    while True:
        is_ongoing, run_id = get_latest_workflow_status(branch)
        
        if not is_ongoing:
            return True
        
        choice = input("\nWorkflow is still running. Wait for completion? (y/n): ").lower()
        if choice != 'y':
            return False
        
        if run_id:
            if monitor_workflow_progress(run_id, branch):
                return True
            else:
                print("Workflow failed. Retrying...")
        else:
            print("Waiting for workflow to start...")
            time.sleep(5)

def get_local_commit_hash(branch_name='main'):
    try:
        result = subprocess.run(['git', 'rev-parse', branch_name], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error fetching commit hash for branch '{branch_name}': {e}")
        return None
def getExecutable(branch):
    run_id, commit_sha = get_latest_successful_run(branch)
    if not run_id:
        return
    
    local_commit_hash = get_local_commit_hash(branch)
    remote_commit_hash = get_remote_commit_hash(branch)
    
    if not remote_commit_hash:
        print("Failed to get remote commit hash")
        return
    
    print(f"Local commit:  {local_commit_hash}")
    print(f"Remote commit: {remote_commit_hash}")
    
    if local_commit_hash != remote_commit_hash:
        print("Warning: Local and remote commits don't match!")
        choice = input("Do you want to continue downloading anyway? (y/n): ").lower()
        if choice != 'y':
            print("Download cancelled")
            return
    
    download_url = get_artifacts_url(run_id)
    if not download_url:
        return
    
    file_name = f"firmware_{branch}_{commit_sha[:8]}.hex"
    print(f"Downloading artifact as: {file_name}")
    
    downloaded_path = download_artifact(download_url, file_name)
    if downloaded_path:
        print(f"Successfully saved to: {downloaded_path}")
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Download GitHub Actions artifacts')
    parser.add_argument('branch', type=str, nargs='?', default='main', help='Branch name to check (default: main)')
    args = parser.parse_args()
    
    if prompt_and_wait(args.branch):
        getExecutable(args.branch)
    else:
        print("Exiting without download.")
""" if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Download GitHub Actions artifacts')
    parser.add_argument('--branch', type=str, default='main', help='Branch name to check')
    args = parser.parse_args()
    
    if get_latest_workflow_status(args.branch):
        print("Workflow is ongoing.")
    else:
        getExecutable(args.branch) """