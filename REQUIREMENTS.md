# Requirements

SCU Student Budget is a desktop budget manager that helps Santa Clara University students track spending and make their money last until their next paycheck, allowance, or quarter.

Team: Josue Diaz Flores, Xiaochao Zhou, Troy Capela, Huy Tran.

## Target users

SCU students who pay for groceries, transportation, dining out, and entertainment with income from part-time jobs, scholarships, or family support. Many on-campus students also manage a meal plan with dining points spent at Benson Dining Hall and the Cellar Market.

## Problems from user research

Interviews with SCU students (P1) showed that:

- Busy students lose track of small, frequent purchases and keep only a mental note of what they spent.
- One-time costs at the start of a quarter, such as parking passes, books, and kitchen supplies, drain savings unexpectedly.
- Students check their bank balance every few days, but they cannot easily tell how much of it is meant for food, hobbies, or other needs.
- Meal point balances are hard to check away from Benson. Some students run out early, while others have points left over at the end of the quarter.
- Students want unobtrusive reminders of how much they have left instead of having to remember to check.

## MVP requirements

These requirements define the minimum viable product. They are the core of a budget manager: set a budget, record spending against it, and see where things stand.

### 1. Create and edit a budget

- The user can enter an initial amount for one overall budget.
- The user can update their budget amount at any time.
- When the budget amount changes, the remaining budget automatically recalculates.

### 2. Log spending

- The user can record an expense by entering an amount and a date, then saving the entry.
- The date defaults to today, but the user can select another date.
- The user receives a clear message if the amount or date is invalid and can correct it before saving.
- When an expense is saved, total spending and the remaining budget automatically update.

### 3. View budget and spending

- The user can see their total budget, total spending, and remaining budget.
- The user can view a list of logged expenses showing each entry's amount and date.
- The remaining budget equals the total budget minus total spending. If spending exceeds the budget, the user sees the negative remaining amount.
- The user's saved budget and spending entries remain available after closing and reopening the application.

## Later requirements

These features came from user research but are not needed for the MVP.

### Category budgets and spending groups

- The user can create separate category budgets, such as groceries, dining out, transportation, and entertainment, within their overall budget.
- The user can assign each expense to a category.
- The user can view the budget, total spending, remaining amount, and logged entries for each category.
- Category budget amounts must stay within the overall budget.

### Edit and recover spending entries

- The user can edit an existing expense's amount, date, and category.
- The user can delete a spending entry and restore recently deleted entries within a recovery period of seven days.
- Editing, deleting, or restoring an entry updates the affected totals and remaining balances.

### Budget periods and income

- The user can choose a budget period: monthly, quarterly, or a custom date range.
- The user can see a daily spending limit that keeps them within their total budget for the period.
- The user can record their income and allocate it to budgets.
- The user can review budgets and spending from previous periods.

### Alerts and reminders

- The user receives an alert when spending approaches or exceeds a budget limit, and can choose the approaching threshold.
- The user can set daily, weekly, or custom reminders of their remaining budget.
- The user can change or turn off alerts and reminders.

### Search and charts

- The user can filter spending entries by date, amount, category, or budget period.
- The user can view spending by category as a pie chart and compare categories or periods with a bar chart.

### SCU dining points

- The user can keep a separate budget and balance for dining points.
- The user can separate dining point spending between Benson Dining Hall and the Cellar Market, and set aside points for the Cellar Market earlier in the quarter.
- Where a supported connection exists, the user can import transactions from their SCU dining account or a payment card without creating duplicate entries.

## Walking skeleton status

The walking skeleton proves the full path from the Qt user interface through the controller to the model, with unit tests running in GitHub Actions.

| Requirement | Status |
| --- | --- |
| Enter a budget amount | Done in the GUI. The skeleton also stores a category and date range for each budget. |
| Update the budget amount | Not started |
| Reject invalid budget input with a message | Done: negative amounts, missing dates, end date before start date, and blank categories |
| Record an expense amount | Done in the model; the GUI has no expense form yet |
| Choose an expense date | Not started |
| Show total budget | Done |
| Show total spending and remaining budget | Done in the model; not shown in the GUI yet |
| Show a list of expenses | Not started |
| Keep data after the app closes | Not started. Data is held in memory for the current session. |
| Show a daily amount for each budget | Done (from the later daily-limit requirement) |
