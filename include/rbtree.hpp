#pragma once
#include <functional>
#include <utility>

template<class Value, class Compare = std::less<Value>>
struct RbTree 
{
	enum RbColor
	{
		red,
		black
	};

	struct RbNode
	{
		RbNode() noexcept
			:left(nullptr), right(nullptr), parent(nullptr), tree(nullptr), color(red)
		{

		}
		RbNode(RbNode&&) = delete;
		~RbNode() noexcept
		{
			if (tree)
			{
				tree->doErase(this);
			}
		}

		friend struct RbTree;
	private:
		RbNode* left;
		RbNode* right;
		RbNode* parent;
		RbTree* tree;
		RbColor color;
	};

private:
	RbNode* root;
	Compare comp;

	bool compare(RbNode* left, RbNode* right) const noexcept
	{
		return comp(static_cast<Value&>(*left), static_cast<Value&>(*right));
	}

	void rotateLeft(RbNode* node) noexcept
	{
		RbNode* rightChild = node->right;
		node->right = rightChild->left;
		if (rightChild->left != nullptr)
		{
			rightChild->left->parent = node;
		}
		rightChild->parent = node->parent;
		if (node->parent == nullptr)
		{
			root = rightChild;
		}
		else if (node == node->parent->left)
		{
			node->parent->left = rightChild;
		}
		else
		{
			node->parent->right = rightChild;
		}
		rightChild->left = node;
		node->parent = rightChild;
	}

	void rotateRight(RbNode* node) noexcept {
		RbNode* leftChild = node->left;
		node->left = leftChild->right;
		if (leftChild->right != nullptr) {
			leftChild->right->parent = node;
		}
		leftChild->parent = node->parent;
		if (node->parent == nullptr) {
			root = leftChild;
		}
		else if (node == node->parent->right) {
			node->parent->right = leftChild;
		}
		else {
			node->parent->left = leftChild;
		}
		leftChild->right = node;
		node->parent = leftChild;
	}

	void fixViolation(RbNode* node) noexcept
	{
		RbNode* parent = nullptr;
		RbNode* grandParent = nullptr;

		//插入红色节点，红黑树只可能出现两种状况
		//一种是根节点为红色
		//一种是出现两个连续的红色节点。

		//处理连续红色节点的情况。
		while (node != root && node->color == red && node->parent->color == red)
		{
			//出现连续红色节点，说明树的深度至少为3，则祖父节点必然存在。

			parent = node->parent;
			grandParent = parent->parent;

			if (parent == grandParent->left)
			{
				RbNode* uncle = grandParent->right;
				
				if (uncle != nullptr && uncle->color == red)
				{
					//叔叔节点是红色，
					//则将叔叔节点和父亲节点变黑，祖父节点变红，
					//检查祖父节点是否违反红黑树性质。

					grandParent->color = red;
					parent->color = black;
					uncle->color = black;
					node = grandParent;
				}
				else
				{
					//叔叔节点是黑色或者没有叔叔节点
					//则判断是LL, LR, RL, RR中的哪种情况。
					//此处父亲节点是左孩子，则对应LL和LR。
					if (node == parent->right)
					{
						//如果是LR类型，则先对父节点进行左旋，使其变成LL类型
						rotateLeft(parent);
						node = parent;
						parent = node->parent;
					}
					//LL类型对祖父节点进行右旋，并让祖父节点和父节点变色
					//由于父节点和祖父节点颜色一定不同，所以此处可以直接交换颜色。
					rotateRight(grandParent);
					std::swap(parent->color, grandParent->color);
					//parent->color = black;
					//grandparent->color = red;
					node = parent;
				}
			}
			else
			{
				//该情况与上面的情况相对称。

				RbNode* uncle = grandParent->left;

				if (uncle != nullptr && uncle->color == red)
				{
					grandParent->color = red;
					parent->color = black;
					uncle->color = black;
					node = grandParent;
				}
				else
				{
					if (node == parent->left)
					{
						rotateRight(parent);
						node = parent;
						parent = node->parent;
					}
					rotateLeft(grandParent);
					std::swap(parent->color, grandParent->color);
					node = parent;
				}
			}
		}
		//若此时根节点为红色，直接修改为黑色
		root->color = black;
	}

	void doInsert(RbNode* node) noexcept
	{
		//默认插入节点为红色

		node->left = nullptr;
		node->right = nullptr;
		node->tree = this;
		node->color = red;

		RbNode* parent = nullptr;
		RbNode* current = root;

		//寻找插入位置
		while (current != nullptr)
		{
			parent = current;
			if (compare(node, current))
			{
				current = current->left;
			}
			else
			{
				current = current->right;
			}
		}
		node->parent = parent;
		//父节点为空，说明此时插入位置为根节点。
		if (parent == nullptr)
		{
			root = node;
		}
		else if (compare(node, parent))
		{
			parent->left = node;
			node->parent = parent;
		}
		else
		{
			parent->right = node;
			node->parent = parent;
		}
		//根据插入后的情况进行调整。
		fixViolation(node);
	}

	void fixLessBlack(RbNode* lessBlackNode) noexcept
	{
		//由于存在递归调用，当lessBlackNode颜色为红色时，可以代表已经完成调节，
		//可以直接将其变为黑色然后返回。
		if (lessBlackNode->color == red)
		{
			lessBlackNode->color = black;
			return;
		}
		//如果为根节点，则不再进行调整
		if (lessBlackNode == root)
		{
			return;
		}

		//当删除没有孩子的黑节点且不是根节点时，首次进入该函数，此时需要考察兄弟节点的情况。
		//此时一定存在兄弟节点，否则会违反红黑树的性质。

		RbNode* parent = lessBlackNode->parent;
		if (lessBlackNode == parent->left)
		{
			
			//当前节点是左孩子，那兄弟节点为右孩子
			RbNode* brother = parent->right;
			RbNode* redChild = nullptr;
			if (brother->color == black)
			{
				if (brother->right != nullptr && brother->right->color == red)
				{
					//兄弟节点存在至少一个红色节点且为右孩子，则此时为RR型
					//需要对其变色，然后再对父节点进行左旋。
					redChild = brother->right;
					redChild->color = brother->color;//红孩子节点颜色变为兄弟节点颜色
					brother->color = parent->color;//兄弟节点颜色变为父节点颜色
					parent->color = black;//父节点变为黑色
					rotateLeft(parent);
				}
				else if (brother->left != nullptr && brother->left->color == red)
				{
					//右孩子不是红色而左孩子是红色，满足RL型
					//此时同样需要变色然后旋转。
					redChild = brother->left;

					redChild->color = parent->color;//红孩子节点颜色变为父节点的颜色
					parent->color = black; // 父节点颜色变为黑色
					rotateRight(brother);//先右旋兄弟节点
					rotateLeft(parent);//再左旋父节点。
				}
				else
				{
					//此时属于兄弟节点全是黑色孩子的情况，
					//选择将兄弟节点变为红色，将父节点当做lessBlackNode进行处理。
					brother->color = red;
					fixLessBlack(parent);
				}
			}
			else
			{
				brother->color = black;
				parent->color = red;
				rotateLeft(parent);
			}
		}
		else
		{
			//此时兄弟节点是左节点
			RbNode* brother = parent->left;
			RbNode* redChild = nullptr;
			if (brother->color == black)
			{
				if (brother->left != nullptr && brother->left->color == red)
				{
					//兄弟节点存在至少一个红色节点且为左孩子，则此时为LL型
					//需要对其变色，然后再对父节点进行右旋。
					redChild = brother->left;
					redChild->color = brother->color;//红孩子节点颜色变为兄弟节点颜色
					brother->color = parent->color;//兄弟节点颜色变为父节点颜色
					parent->color = black;//父节点变为黑色
					rotateRight(parent);
				}
				else if (brother->right != nullptr && brother->right->color == red)
				{
					//右孩子不是红色而左孩子是红色，满足LR型
					//此时同样需要变色然后旋转。
					redChild = brother->right;

					redChild->color = parent->color;//红孩子节点颜色变为父节点的颜色
					parent->color = black; // 父节点颜色变为黑色
					rotateLeft(brother);//先左旋兄弟节点
					rotateRight(parent);//再右旋父节点。
				}
				else
				{
					brother->color = red;
					fixLessBlack(parent);
				}
			}
			else
			{
				brother->color = black;
				parent->color = red;
				rotateRight(parent);
			}
		}
	}

	void doErase(RbNode* current) noexcept
	{
		RbColor color = current->color;
		current->tree = nullptr;
		if (current->left != nullptr && current->right != nullptr)
		{
			/*
			 * 如果被删除节点的左右孩子均存在，则寻找其直接后继节点作为替换节点。
			 */
			RbNode* replace = current;
			replace = replace->right;
			while (replace->left != nullptr)
			{
				replace = replace->left;
			}
			//当前已找到直接后继，对换待删除节点与替换节点。
			std::swap(replace, current);
			//由于swap会将颜色一并替换，需要还原
			current->color = replace->color;
			replace->color = color;
		}
		//此时current节点即为待删除的节点，且只有一个孩子。
		if (current->right || current->left)
		{
			//此时current必然为黑，而current的孩子必然为红。
			//则直接用子节点替换父节点，并将子节点变为黑色。

			RbNode* child = nullptr;
			bool isLeft = current->left != nullptr;
			isLeft ? child = current->left : child = current->right;

			//如果当前待删除节点为根节点时
			if (current == root)
			{
				if (current->left)
				{
					root = current->left;
					root->parent = nullptr;
					root->color = black;
				}
				else if (current->right)
				{
					root = current->right;
					root->parent = nullptr;
					root->color = black;
				}
				else
				{
					root = nullptr;
				}
				current->left = nullptr;
				current->right = nullptr;
			}
			else
			{
				child->parent = current->parent;
				isLeft ? child->parent->left = child : child->parent->right = child;
				current->parent = nullptr;
				current->right = nullptr;
				child->color = black;
			}
		}
		else
		{
			//此时current没有孩子
			//如果为根节点，则可以直接删除
			if (current == root)
			{
				root = nullptr;
				return;
			}
			//如果为红，则直接删除，如果为黑，则再进行判断。
			if (current->color == red)
			{
				RbNode* parent = current->parent;
				current->parent = nullptr;
				current->tree = nullptr;
				parent->left = nullptr;
			}
			else
			{
				fixLessBlack(current);
				current->parent = nullptr;
			}
		}
	}

	RbNode* getFront() const noexcept
	{
		RbNode* current = root;
		while (current->left != nullptr)
		{
			current = current->left;
		}
		return current;
	}

	RbNode* getBack() const noexcept
	{
		RbNode* current = root;
		while (current->right != nullptr)
		{
			current = current->right;
		}
		return  current;
	}

	template<class Visitor>
	void doTraversalInorder(RbNode* node, Visitor&& visitor)
	{
		if (node == nullptr)
		{
			return;
		}
		//中序遍历
		traversalInorder(node->left, visitor);
		visitor(node);
		doTraversalInorder(node->right, visitor);
	}

public:
	RbTree() noexcept : root(nullptr){}
	explicit RbTree(Compare comp) noexcept(noexcept(Compare(comp)))
		:root(nullptr), comp(comp)
	{
		
	}
	RbTree(RbTree&&) = delete;

	~RbTree() noexcept{}
	void insert(Value& value) noexcept
	{
		doInsert(&static_cast<RbNode&>(value));
	}
	void erase(Value& value) noexcept {
		doErase(&static_cast<RbNode&>(value));
	}

	bool empty() const noexcept {
		return root == nullptr;
	}

	Value& front() const noexcept
	{
		return static_cast<Value&>(*getFront());
	}

	Value& back() const noexcept
	{
		return static_cast<Value&>(*getBack());
	}

	template<class Visitor>
	void traversalInorder(Visitor&& visitor)
	{
		doTraversalInorder(root, std::forward<Visitor>(visitor));
	}
};
