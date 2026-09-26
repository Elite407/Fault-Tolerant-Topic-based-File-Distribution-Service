# Team Collaboration & Git Workflow

To ensure smooth collaboration without overwriting each other's work, our team uses a **Branch-and-Merge** workflow along with Continuous Integration (CI).

## 1. Initial Setup
Clone the repository to your local machine:
```bash
git clone https://github.com/Elite407/Fault-Tolerant-Topic-based-File-Distribution-Service.git
cd Fault-Tolerant-Topic-based-File-Distribution-Service
```

## 2. Working on Your Part (Branching)
**Never commit directly to the `main` branch.** Always create a new branch for your specific feature or task.

For example, if you are working on the client logic in Part 1:
```bash
# 1. Get the latest changes from main
git checkout main
git pull origin main

# 2. Create and switch to a new branch for your work
git checkout -b feature/part1-client
```

## 3. Saving Your Work (Committing)
As you write code, save your changes locally with descriptive commit messages:
```bash
# See what files you've modified
git status

# Stage the files you want to commit (e.g., your specific file)
git add Part1/client.c

# Commit with a clear message explaining what you did
git commit -m "Implement GET request logic in client"
```

## 4. Pushing to GitHub
When you are ready to share your work or trigger the automated tests, push your branch to GitHub:
```bash
git push origin feature/part1-client
```

## 5. Automated Testing (CI/CD)
We have set up a GitHub Actions CI/CD pipeline (located in `.github/workflows/build.yml`). 
Whenever you push a branch or create a Pull Request, GitHub will automatically spin up an Ubuntu server and attempt to compile your code using the `Makefile`.

*   **Success:** If your code compiles without syntax errors, you will see a **Green Checkmark** ✅ next to your commit.
*   **Failure:** If there are compilation errors, you will see a **Red Cross** ❌. You must fix the errors locally, commit the fixes, and push again.

*Note: The CI pipeline currently only checks if it can run `make` successfully inside the directories.*

## 6. Merging Your Work (Pull Requests)
Once your code is working and the CI tests pass:
1. Go to the repository on GitHub.
2. Click **"Compare & pull request"** next to your branch.
3. Add a brief description of what you completed.
4. Have at least one other team member review your code.
5. Once approved, click **"Merge pull request"** to integrate your code into the `main` branch.
